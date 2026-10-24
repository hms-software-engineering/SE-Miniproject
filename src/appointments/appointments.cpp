#include "appointments/appointments.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace hms {

namespace {

const char* const kDayNames[7] = {"sunday",   "monday", "tuesday", "wednesday",
                                  "thursday", "friday", "saturday"};

bool valid_date(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;

    for (size_t i = 0; i < d.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(d[i]))) return false;
    }

    int year = std::stoi(d.substr(0, 4));
    int month = std::stoi(d.substr(5, 2));
    int day = std::stoi(d.substr(8, 2));

    if (month < 1 || month > 12) return false;

    const int days_in_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    int max_day = days_in_month[month - 1];
    bool leap = (year % 400 == 0) ||
                (year % 4 == 0 && year % 100 != 0);

    if (month == 2 && leap) max_day = 29;

    return day >= 1 && day <= max_day;
}
bool parse_hhmm(const std::string& s, int& minutes);
bool valid_booking_datetime(const std::string& date,
                           const std::string& time) {
    int minutes = 0;
    if (!valid_date(date) || !parse_hhmm(time, minutes)) return false;

    std::tm requested = {};
    std::istringstream input(date + " " + time);
    input >> std::get_time(&requested, "%Y-%m-%d %H:%M");
    if (input.fail()) return false;

    std::time_t now_time = std::time(nullptr);
    std::tm now_local = *std::localtime(&now_time);

    std::tm today = now_local;
    today.tm_hour = 0;
    today.tm_min = 0;
    today.tm_sec = 0;

    requested.tm_isdst = -1;
    today.tm_isdst = -1;

    std::time_t requested_time = std::mktime(&requested);
    std::time_t today_time = std::mktime(&today);

    if (requested_time == static_cast<std::time_t>(-1) ||
        today_time == static_cast<std::time_t>(-1)) {
        return false;
    }

    // Reject any appointment earlier than the current time.
    if (requested_time <= now_time) return false;

    // Permit dates from today through 60 calendar days ahead.
    std::tm latest = today;
    latest.tm_mday += 60;
    latest.tm_isdst = -1;
    std::time_t latest_time = std::mktime(&latest);

    std::tm requested_day = requested;
    requested_day.tm_hour = 0;
    requested_day.tm_min = 0;
    requested_day.tm_sec = 0;
    requested_day.tm_isdst = -1;
    std::time_t requested_day_time = std::mktime(&requested_day);

    return requested_day_time >= today_time &&
           requested_day_time <= latest_time;
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
                               const std::string& date, const std::string& time) {
    if (patient_id.empty())
        return fail(400, "VALIDATION_FAILED", "patientId is required", "patientId");
    if (doctor_id <= 0)
        return fail(400, "VALIDATION_FAILED", "doctorId is required", "doctorId");
    if (!valid_date(date) || !valid_booking_datetime(date, time)) {
        return fail(400, "INVALID_DATE",
                    "Appointment date/time must be in the future and within 60 days",
                    "slotStart");
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

    // F-021 will add the next 3 free slots to this response.
    SaveResult s = repo.insert(a);
    switch (s.status) {
    case SaveStatus::SlotTaken:
        return fail(409, "SLOT_TAKEN", "Slot is no longer available");
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
