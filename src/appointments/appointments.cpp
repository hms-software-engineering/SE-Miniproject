#include "appointments/appointments.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
struct pg_conn;
struct pg_result;
using PGconn = pg_conn;
using PGresult = pg_result;
#else
#include <libpq-fe.h>
#endif

namespace hms {

namespace {

constexpr int CONNECTION_OK = 0;
constexpr int PGRES_COMMAND_OK = 1;
constexpr int PGRES_TUPLES_OK = 2;

#ifdef _WIN32
struct PostgresApi {
    HMODULE library = nullptr;
    PGconn* (__cdecl* connectdb)(const char*) = nullptr;
    int (__cdecl* status)(const PGconn*) = nullptr;
    char* (__cdecl* error_message)(const PGconn*) = nullptr;
    void (__cdecl* finish)(PGconn*) = nullptr;
    PGresult* (__cdecl* exec_params)(
        PGconn*, const char*, int, const unsigned int*,
        const char* const*, const int*, const int*, int) = nullptr;
    int (__cdecl* result_status)(const PGresult*) = nullptr;
    char* (__cdecl* result_error_message)(const PGresult*) = nullptr;
    int (__cdecl* tuples)(const PGresult*) = nullptr;
    char* (__cdecl* value)(const PGresult*, int, int) = nullptr;
    void (__cdecl* clear)(PGresult*) = nullptr;

    PostgresApi()
    {
        library = LoadLibraryA("libpq.dll");
        if (library == nullptr) {
            throw std::runtime_error(
                "Could not load libpq.dll; add PostgreSQL's bin directory "
                "to PATH.");
        }
        try {
            connectdb = load<decltype(connectdb)>("PQconnectdb");
            status = load<decltype(status)>("PQstatus");
            error_message = load<decltype(error_message)>("PQerrorMessage");
            finish = load<decltype(finish)>("PQfinish");
            exec_params = load<decltype(exec_params)>("PQexecParams");
            result_status = load<decltype(result_status)>("PQresultStatus");
            result_error_message =
                load<decltype(result_error_message)>("PQresultErrorMessage");
            tuples = load<decltype(tuples)>("PQntuples");
            value = load<decltype(value)>("PQgetvalue");
            clear = load<decltype(clear)>("PQclear");
        } catch (...) {
            FreeLibrary(library);
            library = nullptr;
            throw;
        }
    }

    ~PostgresApi()
    {
        if (library != nullptr) FreeLibrary(library);
    }

private:
    template <typename Function>
    Function load(const char* name)
    {
        const auto address = GetProcAddress(library, name);
        if (address == nullptr) {
            throw std::runtime_error(
                std::string("PostgreSQL client is missing symbol ") + name);
        }
        static_assert(sizeof(Function) == sizeof(address),
                      "Windows API function pointers must have equal size.");
        Function function = nullptr;
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }
};

PostgresApi& postgres_api()
{
    static PostgresApi api;
    return api;
}

PGconn* pg_connectdb(const char* value)
{
    return postgres_api().connectdb(value);
}
int pg_status(const PGconn* value)
{
    return postgres_api().status(value);
}
char* pg_error_message(const PGconn* value)
{
    return postgres_api().error_message(value);
}
void pg_finish(PGconn* value)
{
    postgres_api().finish(value);
}
PGresult* pg_exec_params(PGconn* connection,
                         const char* command,
                         int count,
                         const char* const* values)
{
    return postgres_api().exec_params(
        connection, command, count, nullptr, values, nullptr, nullptr, 0);
}
int pg_result_status(const PGresult* value)
{
    return postgres_api().result_status(value);
}
char* pg_result_error_message(const PGresult* value)
{
    return postgres_api().result_error_message(value);
}
int pg_tuples(const PGresult* value)
{
    return postgres_api().tuples(value);
}
char* pg_value(const PGresult* value, int row, int column)
{
    return postgres_api().value(value, row, column);
}
void pg_clear(PGresult* value)
{
    postgres_api().clear(value);
}
#else
PGconn* pg_connectdb(const char* value) { return PQconnectdb(value); }
int pg_status(const PGconn* value) { return PQstatus(value); }
char* pg_error_message(const PGconn* value) { return PQerrorMessage(value); }
void pg_finish(PGconn* value) { PQfinish(value); }
PGresult* pg_exec_params(PGconn* connection,
                         const char* command,
                         int count,
                         const char* const* values)
{
    return PQexecParams(connection, command, count, nullptr, values, nullptr,
                        nullptr, 0);
}
int pg_result_status(const PGresult* value) { return PQresultStatus(value); }
char* pg_result_error_message(const PGresult* value)
{
    return PQresultErrorMessage(value);
}
int pg_tuples(const PGresult* value) { return PQntuples(value); }
char* pg_value(const PGresult* value, int row, int column)
{
    return PQgetvalue(value, row, column);
}
void pg_clear(PGresult* value) { PQclear(value); }
#endif

class PgResult {
public:
    explicit PgResult(PGresult* value) : value_(value) {}
    ~PgResult()
    {
        if (value_ != nullptr) pg_clear(value_);
    }
    PgResult(const PgResult&) = delete;
    PgResult& operator=(const PgResult&) = delete;
    PGresult* get() const { return value_; }

private:
    PGresult* value_;
};

void require_pg_status(PGconn* connection,
                       PGresult* result,
                       int expected,
                       const char* operation)
{
    if (result == nullptr || pg_result_status(result) != expected) {
        const std::string details = result == nullptr
            ? pg_error_message(connection)
            : pg_result_error_message(result);
        if (result != nullptr) pg_clear(result);
        throw std::runtime_error(std::string(operation) + ": " + details);
    }
}

PGresult* execute(PGconn* connection,
                  const char* sql,
                  const std::vector<std::string>& values)
{
    std::vector<const char*> parameters;
    parameters.reserve(values.size());
    for (const auto& value : values) parameters.push_back(value.c_str());
    return pg_exec_params(connection, sql,
                          static_cast<int>(parameters.size()),
                          parameters.empty() ? nullptr : parameters.data());
}

const char* const kDayNames[7] = {"sunday",   "monday", "tuesday", "wednesday",
                                  "thursday", "friday", "saturday"};

bool parse_date(const std::string& d, int& year, int& month, int& day) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (size_t i = 0; i < d.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(d[i]))) return false;
    }
    year = std::stoi(d.substr(0, 4));
    month = std::stoi(d.substr(5, 2));
    day = std::stoi(d.substr(8, 2));
    if (month < 1 || month > 12) return false;
    static constexpr int month_days[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year % 4 == 0 &&
                      (year % 100 != 0 || year % 400 == 0);
    const int max_day = month_days[month - 1] +
                        (month == 2 && leap ? 1 : 0);
    return day >= 1 && day <= max_day;
}

bool valid_date(const std::string& value) {
    int year = 0, month = 0, day = 0;
    return parse_date(value, year, month, day);
}

long long days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month =
        month > 2 ? month - 3 : month + 9;
    const unsigned day_of_year =
        (153 * adjusted_month + 2) / 5 + day - 1;
    const unsigned day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 +
        day_of_year;
    return era * 146097LL + static_cast<long long>(day_of_era);
}

std::string date_after(const std::string& date, int days) {
    int year = 0, month = 0, day = 0;
    parse_date(date, year, month, day);
    std::tm value{};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day + days;
    std::mktime(&value);
    char output[11]{};
    std::strftime(output, sizeof(output), "%Y-%m-%d", &value);
    return output;
}

std::tm local_time(std::chrono::system_clock::time_point point) {
    const std::time_t timestamp =
        std::chrono::system_clock::to_time_t(point);
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &timestamp);
#else
    localtime_r(&timestamp, &result);
#endif
    return result;
}

std::string format_date(const std::tm& value) {
    char output[11]{};
    std::strftime(output, sizeof(output), "%Y-%m-%d", &value);
    return output;
}

std::string format_time(const std::tm& value) {
    char output[6]{};
    std::strftime(output, sizeof(output), "%H:%M", &value);
    return output;
}

std::vector<std::string> upcoming_slots(const AppointmentRepository& repo,
                                        long long doctor_id,
                                        const std::string& date,
                                        const std::string& time,
                                        std::chrono::system_clock::time_point now) {
    std::vector<std::string> result;
    const std::tm current = local_time(now);
    const std::string today = format_date(current);
    const std::string current_time = format_time(current);
    for (int offset = 0; offset <= 60 && result.size() < 3; ++offset) {
        const std::string candidate_date = date_after(date, offset);
        for (const auto& slot : free_slots(repo, doctor_id, candidate_date)) {
            if (offset == 0 && slot <= time) continue;
            if (candidate_date == today && slot <= current_time) continue;
            result.push_back(candidate_date + " " + slot);
            if (result.size() == 3) break;
        }
    }
    return result;
}

// Reads the "HH:MM-HH:MM" value for a day name from the JSON text.
std::string range_for_day(const std::string& json, const std::string& day) {
    std::string key = "\"" + day + "\"";
    size_t k = json.find(key);
    if (k == std::string::npos) return "";
    size_t colon = json.find(':', k + key.size());
    if (colon == std::string::npos) return "";
    size_t q1 = json.find('"', colon);
    if (q1 == std::string::npos) return "";
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    return json.substr(q1 + 1, q2 - q1 - 1);
}

bool parse_hhmm(const std::string& s, int& minutes) {
    if (s.size() != 5 || s[2] != ':') return false;
    for (size_t i : {0u, 1u, 3u, 4u})
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    int h = std::stoi(s.substr(0, 2));
    int m = std::stoi(s.substr(3, 2));
    if (h > 23 || m > 59) return false;
    minutes = h * 60 + m;
    return true;
}

BookingResult fail(int status, const std::string& code, const std::string& msg,
                   const std::string& field = "") {
    BookingResult r;
    r.http_status = status;
    r.code = code;
    r.message = msg;
    r.field = field;
    return r;
}

}  // namespace

struct PostgresAppointmentRepository::Connection {
    PGconn* value = nullptr;
};

PostgresAppointmentRepository::PostgresAppointmentRepository(
    const char* connection_info)
    : connection_(new Connection)
{
    connection_->value =
        pg_connectdb(connection_info == nullptr ? "" : connection_info);
    if (connection_->value == nullptr ||
        pg_status(connection_->value) != CONNECTION_OK) {
        const std::string message = connection_->value == nullptr
            ? "Could not allocate PostgreSQL connection."
            : pg_error_message(connection_->value);
        if (connection_->value != nullptr) pg_finish(connection_->value);
        delete connection_;
        connection_ = nullptr;
        throw std::runtime_error("PostgreSQL connection failed: " + message);
    }
}

PostgresAppointmentRepository::~PostgresAppointmentRepository()
{
    if (connection_ != nullptr) {
        pg_finish(connection_->value);
        delete connection_;
    }
}

std::string PostgresAppointmentRepository::working_hours(
    long long doctor_id) const
{
    const auto id = std::to_string(doctor_id);
    const std::vector<std::string> values{id};
    PGresult* raw = execute(
        connection_->value,
        "SELECT working_hours::text FROM doctors WHERE doctor_id = $1::bigint",
        values);
    require_pg_status(connection_->value, raw, PGRES_TUPLES_OK,
                      "Could not read doctor schedule");
    PgResult result(raw);
    if (pg_tuples(result.get()) == 0) return {};
    return pg_value(result.get(), 0, 0);
}

std::vector<std::string> PostgresAppointmentRepository::booked_times(
    long long doctor_id, const std::string& date) const
{
    const std::vector<std::string> values{
        std::to_string(doctor_id), date};
    PGresult* raw = execute(
        connection_->value,
        "SELECT to_char(slot_start, 'HH24:MI') FROM appointments "
        "WHERE doctor_id = $1::bigint AND slot_start::date = $2::date "
        "AND status = 'Confirmed' ORDER BY slot_start",
        values);
    require_pg_status(connection_->value, raw, PGRES_TUPLES_OK,
                      "Could not read booked appointment slots");
    PgResult result(raw);
    std::vector<std::string> times;
    for (int row = 0; row < pg_tuples(result.get()); ++row) {
        times.emplace_back(pg_value(result.get(), row, 0));
    }
    return times;
}

SaveResult PostgresAppointmentRepository::insert(
    const Appointment& appointment)
{
    const std::vector<std::string> values{
        appointment.patient_id,
        std::to_string(appointment.doctor_id),
        appointment.slot_start
    };
    PGresult* raw = execute(
        connection_->value,
        "INSERT INTO appointments (patient_id, doctor_id, slot_start, status) "
        "VALUES ($1, $2::bigint, $3::timestamp, 'Confirmed') "
        "ON CONFLICT (doctor_id, slot_start) WHERE status = 'Confirmed' "
        "DO NOTHING RETURNING appointment_id::text",
        values);
    require_pg_status(connection_->value, raw, PGRES_TUPLES_OK,
                      "Could not book appointment");
    PgResult result(raw);
    if (pg_tuples(result.get()) == 0) {
        return {SaveStatus::SlotTaken, {}};
    }
    Appointment saved = appointment;
    saved.id = std::stoll(pg_value(result.get(), 0, 0));
    saved.status = "Confirmed";
    return {SaveStatus::Ok, std::move(saved)};
}

bool PostgresAppointmentRepository::patient_belongs_to_user(
    const std::string& patient_id, const std::string& user_id) const
{
    const std::vector<std::string> values{patient_id, user_id};
    PGresult* raw = execute(
        connection_->value,
        "SELECT 1 FROM patients WHERE patient_id = $1 AND user_id = $2::bigint",
        values);
    require_pg_status(connection_->value, raw, PGRES_TUPLES_OK,
                      "Could not verify patient ownership");
    PgResult result(raw);
    return pg_tuples(result.get()) != 0;
}

std::string PostgresAppointmentRepository::doctor_id_for_user(
    const std::string& user_id) const
{
    const std::vector<std::string> values{user_id};
    PGresult* raw = execute(
        connection_->value,
        "SELECT doctor_id::text FROM doctors WHERE user_id = $1::bigint",
        values);
    require_pg_status(connection_->value, raw, PGRES_TUPLES_OK,
                      "Could not read doctor profile");
    PgResult result(raw);
    if (pg_tuples(result.get()) == 0) return {};
    return pg_value(result.get(), 0, 0);
}

// ---------- in-memory repository ----------

InMemoryAppointmentRepository::InMemoryAppointmentRepository() {
    doctors_[1] =
        "{\"monday\":\"09:00-13:00\",\"wednesday\":\"09:00-13:00\","
        "\"friday\":\"14:00-17:00\"}";
    patients_.insert("PID-00000001");
    patients_.insert("PID-00000002");
}

std::string InMemoryAppointmentRepository::working_hours(long long doctor_id) const {
    auto it = doctors_.find(doctor_id);
    return it == doctors_.end() ? "" : it->second;
}

std::vector<std::string> InMemoryAppointmentRepository::booked_times(
    long long doctor_id, const std::string& date) const {
    std::vector<std::string> out;
    for (const auto& a : items_) {
        if (a.doctor_id == doctor_id && a.status == "Confirmed" &&
            a.slot_start.substr(0, 10) == date)
            out.push_back(a.slot_start.substr(11));
    }
    return out;
}

SaveResult InMemoryAppointmentRepository::insert(const Appointment& a) {
    SaveResult r;
    if (patients_.find(a.patient_id) == patients_.end()) {
        r.status = SaveStatus::PatientMissing;
        return r;
    }
    if (doctors_.find(a.doctor_id) == doctors_.end()) {
        r.status = SaveStatus::DoctorMissing;
        return r;
    }
    for (const auto& x : items_) {
        if (x.doctor_id == a.doctor_id && x.status == "Confirmed" &&
            x.slot_start == a.slot_start) {
            r.status = SaveStatus::SlotTaken;
            return r;
        }
    }
    Appointment saved = a;
    saved.id = next_id_++;
    saved.status = "Confirmed";
    items_.push_back(saved);
    r.status = SaveStatus::Ok;
    r.appointment = saved;
    return r;
}

// ---------- logic ----------

int weekday_of(const std::string& date) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = std::stoi(date.substr(0, 4));
    int m = std::stoi(date.substr(5, 2));
    int d = std::stoi(date.substr(8, 2));
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

bool appointment_date_valid(const std::string& date)
{
    int year = 0, month = 0, day = 0;
    return parse_date(date, year, month, day);
}

std::vector<std::string> slots_for_date(const std::string& working_hours_json,
                                        const std::string& date) {
    std::vector<std::string> out;
    if (!valid_date(date)) return out;
    std::string range = range_for_day(working_hours_json, kDayNames[weekday_of(date)]);
    size_t dash = range.find('-');
    if (dash == std::string::npos) return out;
    int start = 0, end = 0;
    if (!parse_hhmm(range.substr(0, dash), start) ||
        !parse_hhmm(range.substr(dash + 1), end))
        return out;
    for (int m = start; m + 30 <= end; m += 30) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
        out.push_back(buf);
    }
    return out;
}

std::vector<std::string> free_slots(const AppointmentRepository& repo,
                                    long long doctor_id, const std::string& date) {
    std::vector<std::string> out;
    std::string hours = repo.working_hours(doctor_id);
    if (hours.empty()) return out;
    std::vector<std::string> booked = repo.booked_times(doctor_id, date);
    for (const auto& s : slots_for_date(hours, date)) {
        if (std::find(booked.begin(), booked.end(), s) == booked.end())
            out.push_back(s);
    }
    return out;
}

BookingResult book_appointment(AppointmentRepository& repo,
                               const std::string& patient_id, long long doctor_id,
                               const std::string& date, const std::string& time,
                               std::chrono::system_clock::time_point now) {
    if (patient_id.empty())
        return fail(400, "VALIDATION_FAILED", "patientId is required", "patientId");
    if (doctor_id <= 0)
        return fail(400, "VALIDATION_FAILED", "doctorId is required", "doctorId");
    if (!valid_date(date))
        return fail(400, "VALIDATION_FAILED", "date must be YYYY-MM-DD", "date");
    int year = 0, month = 0, day = 0;
    parse_date(date, year, month, day);
    const std::tm current = local_time(now);
    const std::string today = format_date(current);
    int today_year = 0, today_month = 0, today_day = 0;
    parse_date(today, today_year, today_month, today_day);
    const long long days_ahead =
        days_from_civil(year, static_cast<unsigned>(month),
                        static_cast<unsigned>(day)) -
        days_from_civil(today_year, static_cast<unsigned>(today_month),
                        static_cast<unsigned>(today_day));
    if (days_ahead < 0 ||
        (days_ahead == 0 && time <= format_time(current))) {
        return fail(400, "BOOKING_IN_PAST",
                    "Choose a date and time that have not passed.", "date");
    }
    if (days_ahead > 60) {
        return fail(400, "BOOKING_TOO_FAR",
                    "Appointments can be booked no more than 60 days ahead.",
                    "date");
    }

    std::string hours = repo.working_hours(doctor_id);
    if (hours.empty()) return fail(404, "NOT_FOUND", "Doctor not found", "doctorId");

    auto slots = slots_for_date(hours, date);
    if (std::find(slots.begin(), slots.end(), time) == slots.end())
        return fail(400, "VALIDATION_FAILED",
                    "time is not a working slot for this doctor", "time");

    Appointment a;
    a.patient_id = patient_id;
    a.doctor_id = doctor_id;
    a.slot_start = date + " " + time;
    a.status = "Confirmed";

    SaveResult s = repo.insert(a);
    switch (s.status) {
    case SaveStatus::SlotTaken: {
        BookingResult result =
            fail(409, "SLOT_TAKEN", "Slot is no longer available");
        result.suggested_slots =
            upcoming_slots(repo, doctor_id, date, time, now);
        return result;
    }
    case SaveStatus::PatientMissing:
        return fail(400, "VALIDATION_FAILED", "Patient not found", "patientId");
    case SaveStatus::DoctorMissing:
        return fail(404, "NOT_FOUND", "Doctor not found", "doctorId");
    case SaveStatus::Ok:
        break;
    }
    BookingResult r;
    r.ok = true;
    r.http_status = 201;
    r.appointment = s.appointment;
    return r;
}

std::string slots_to_json(const std::vector<std::string>& slots) {
    std::string j = "{\"slots\":[";
    for (size_t i = 0; i < slots.size(); ++i) {
        if (i) j += ",";
        j += "\"" + slots[i] + "\"";
    }
    return j + "]}";
}

std::string appointment_to_json(const Appointment& a) {
    return "{\"appointmentId\":" + std::to_string(a.id) + ",\"patientId\":\"" +
           a.patient_id + "\",\"doctorId\":" + std::to_string(a.doctor_id) +
           ",\"slotStart\":\"" + a.slot_start + "\",\"status\":\"" + a.status + "\"}";
}

}  // namespace hms
