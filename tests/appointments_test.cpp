
#include "../src/appointments/appointments.h"
#include "test_date_utils.h"

#include <cstdlib>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>

#define CHECK(c) do { \
    if (!(c)) { \
        std::cerr << "FAILED: " #c " (line " << __LINE__ << ")\n"; \
        std::exit(1); \
    } \
} while (0)

static std::string date_offset(int days) {
    std::tm local{};
    local.tm_year = 2026 - 1900;
    local.tm_mon = 9;
    local.tm_mday = 25;
    local.tm_hour = 12;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_mday += days;
    local.tm_isdst = -1;

    std::time_t target = std::mktime(&local);
    std::tm result = *std::localtime(&target);

    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &result);
    return buffer;
}

static int weekday(const std::string& date) {
    return hms::weekday_of(date);
}

static std::string integration_test_date() {
    return test_support::integration_test_date();
}

static void test_postgres_repository_when_enabled() {
    const char* enabled = std::getenv("HMS_APPOINTMENT_TEST_POSTGRES");
    if (enabled == nullptr || std::string(enabled) != "1") return;

    hms::PostgresAppointmentRepository repository;
    const std::string date = integration_test_date();
    CHECK(!repository.working_hours(1).empty());
    CHECK(repository.working_hours(999999).empty());

    hms::Appointment appointment{0, "PID-00000001", 1, date + " 09:00", {}};
    const auto saved = repository.insert(appointment);
    CHECK(saved.status == hms::SaveStatus::Ok);
    CHECK(saved.appointment.id > 0);
    CHECK(saved.appointment.status == "Confirmed");

    const auto duplicate = repository.insert(appointment);
    CHECK(duplicate.status == hms::SaveStatus::SlotTaken);
    CHECK(repository.booked_times(1, date).size() == 1);
    const auto scheduled = repository.appointments_for_doctor_on(1, date);
    CHECK(scheduled.size() == 1);
    CHECK(scheduled.front().id == saved.appointment.id);
    CHECK(repository.patient_belongs_to_user("PID-00000001", "3"));
    CHECK(!repository.patient_belongs_to_user("PID-00000001", "4"));
    CHECK(repository.doctor_id_for_user("2") == "1");
    CHECK(repository.doctor_id_for_user("999999").empty());
    const auto report = repository.daily_report(date, date);
    CHECK(report.size() == 1);
    CHECK(report.front().appointment_count == 1);
}

namespace {

std::chrono::system_clock::time_point test_now()
{
    std::tm time{};
    time.tm_year = 2026 - 1900;
    time.tm_mon = 9;
    time.tm_mday = 25;
    time.tm_hour = 8;
    return std::chrono::system_clock::from_time_t(std::mktime(&time));
}

}  // namespace

class BoundaryTestRepository : public hms::AppointmentRepository {
public:
    std::string working_hours(long long doctor_id) const override {
        if (doctor_id != 1) return "";
        // Give doctor 1 working hours on every weekday so the
        // day-60 boundary test can never be skipped.
        return R"({"sunday":"09:00-13:00",
                   "monday":"09:00-13:00",
                   "tuesday":"09:00-13:00",
                   "wednesday":"09:00-13:00",
                   "thursday":"09:00-13:00",
                   "friday":"09:00-13:00",
                   "saturday":"09:00-13:00"})";
    }

    std::vector<std::string> booked_times(
        long long, const std::string&) const override {
        return {};
    }

    hms::SaveResult insert(const hms::Appointment& a) override {
        hms::SaveResult result;
        result.status = hms::SaveStatus::Ok;
        result.appointment = a;
        result.appointment.id = next_id_++;
        result.appointment.status = "Confirmed";
        return result;
    }
    bool find_by_id(long long, hms::Appointment&) const override {
        return false;
    }
    bool update_status(long long, const std::string&) override {
        return false;
    }
    hms::SaveStatus reschedule(long long, const std::string&) override {
        return hms::SaveStatus::PatientMissing;
    }

    std::vector<hms::Appointment> appointments_for_doctor_on(
    long long doctor_id,
    const std::string& date) const override
    {
        (void)doctor_id;
        (void)date;
        return {};
    }

    std::string doctor_id_for_user(
        const std::string& user_id) const override
    {
        (void)user_id;
        return {};
    }

private:
    long long next_id_ = 1;
};

int main() {
    hms::InMemoryAppointmentRepository repo;

    CHECK(test_support::date_from_half_hour_bucket(0) == "2000-01-01");
    CHECK(test_support::date_from_half_hour_bucket(58) == "2000-02-28");
    CHECK(test_support::date_from_half_hour_bucket(59) == "2000-02-29");
    CHECK(test_support::date_from_half_hour_bucket(60) == "2000-03-01");
    CHECK(test_support::date_from_half_hour_bucket(365) == "2000-12-31");
    CHECK(test_support::date_from_half_hour_bucket(366) == "2001-01-01");
    CHECK(test_support::date_from_half_hour_bucket(9999) == "2027-05-18");
    CHECK(test_support::date_from_half_hour_bucket(10000) == "2000-01-01");

    // Existing weekday and slot-generation behavior.
    CHECK(weekday("2026-10-26") == 1);
    CHECK(hms::free_slots(repo, 1, "2026-10-27").empty());
    CHECK(hms::free_slots(repo, 1, "2026-10-30").size() == 6);

    // Find a future Monday for successful booking tests.
    std::string monday;
    for (int offset = 1; offset <= 60; ++offset) {
        std::string candidate = date_offset(offset);
        if (weekday(candidate) == 1) {
            monday = candidate;
            break;
        }
    }
    CHECK(!monday.empty());

    auto r = hms::book_appointment(
        repo, "PID-00000001", 1, monday, "09:30", test_now());
    CHECK(r.ok);
    CHECK(r.http_status == 201);
    CHECK(r.appointment.status == "Confirmed");
    CHECK(r.appointment.id == 1);

    auto r2 = hms::book_appointment(
        repo, "PID-00000002", 1, monday, "10:00", test_now());
    CHECK(r2.ok);
    CHECK(r2.appointment.id != r.appointment.id);

    // Same slot twice is refused.
    auto dup = hms::book_appointment(
        repo, "PID-00000002", 1, monday, "09:30", test_now());
    CHECK(!dup.ok);
    CHECK(dup.http_status == 409);
    CHECK(dup.suggested_slots.size() == 3);
    CHECK(dup.suggested_slots[0] == "2026-10-26 10:30");
    // Required-field and general validation.
    CHECK(hms::book_appointment(
        repo, "", 1, monday, "11:00", test_now()).field == "patientId");

    auto bad_format = hms::book_appointment(
        repo, "PID-00000001", 1, "26-10-2026", "11:00", test_now());
    CHECK(!bad_format.ok);
    CHECK(bad_format.http_status == 400);
    CHECK(bad_format.code == "INVALID_DATE");

    auto impossible_date = hms::book_appointment(
        repo, "PID-00000001", 1, "2026-02-31", "11:00", test_now());
    CHECK(!impossible_date.ok);
    CHECK(impossible_date.code == "INVALID_DATE");

    // Non-leap year: February 29 must be rejected.
    auto non_leap_day = hms::book_appointment(
        repo, "PID-00000001", 1, "2026-02-29", "11:00", test_now());
    CHECK(!non_leap_day.ok);
    CHECK(non_leap_day.http_status == 400);
    CHECK(non_leap_day.code == "INVALID_DATE");

    // Leap-year calendar validation.
    CHECK(weekday("2028-02-29") >= 0);

    // April has only 30 days.
    auto april_31 = hms::book_appointment(
        repo, "PID-00000001", 1, "2026-04-31", "11:00", test_now());
    CHECK(!april_31.ok);
    CHECK(april_31.http_status == 400);
    CHECK(april_31.code == "INVALID_DATE");

    // Invalid month and day values.
    auto invalid_month = hms::book_appointment(
        repo, "PID-00000001", 1, "2026-13-01", "11:00", test_now());
    CHECK(!invalid_month.ok);
    CHECK(invalid_month.code == "INVALID_DATE");

    auto invalid_day = hms::book_appointment(
        repo, "PID-00000001", 1, "2026-01-00", "11:00", test_now());
    CHECK(!invalid_day.ok);
    CHECK(invalid_day.code == "INVALID_DATE");

    auto past = hms::book_appointment(
        repo, "PID-00000001", 1, date_offset(-1), "11:00", test_now());
    CHECK(!past.ok);
    CHECK(past.http_status == 400);
    CHECK(past.code == "INVALID_DATE");

    auto elapsed_today = hms::book_appointment(
        repo, "PID-00000001", 1, date_offset(0), "00:00", test_now());
    CHECK(!elapsed_today.ok);
    CHECK(elapsed_today.http_status == 400);
    CHECK(elapsed_today.field == "slotStart");

    auto too_far = hms::book_appointment(
        repo, "PID-00000001", 1, date_offset(61), "11:00", test_now());
    CHECK(!too_far.ok);
    CHECK(too_far.http_status == 400);
    CHECK(too_far.code == "INVALID_DATE");

    // Day 60: the date boundary itself is allowed.
    BoundaryTestRepository boundary_repo;
    std::string day60 = date_offset(60);

    auto boundary = hms::book_appointment(
        boundary_repo, "PID-00000001", 1, day60, "09:00", test_now());

    CHECK(boundary.ok);
    CHECK(boundary.http_status == 201);
    CHECK(boundary.appointment.slot_start == day60 + " 09:00");

    // Day 61 must be rejected.
    auto day61 = hms::book_appointment(
        boundary_repo, "PID-00000001", 1, date_offset(61), "09:00",
        test_now());

    CHECK(!day61.ok);
    CHECK(day61.http_status == 400);
    CHECK(day61.code == "INVALID_DATE");
    CHECK(day61.field == "slotStart");

    // Calendar-date boundary validation.
    CHECK(!hms::is_valid_calendar_date("26-10-2026"));
    CHECK(!hms::is_valid_calendar_date("2026-02-31"));
    CHECK(!hms::is_valid_calendar_date("2026-02-29"));
    CHECK(hms::is_valid_calendar_date("2028-02-29"));
    CHECK(!hms::is_valid_calendar_date("2026-04-31"));
    CHECK(!hms::is_valid_calendar_date("2026-13-01"));
    CHECK(!hms::is_valid_calendar_date("2026-01-00"));
    CHECK(!hms::is_valid_calendar_date("2026-00-10"));
    CHECK(!hms::is_valid_calendar_date("0000-01-01"));
    CHECK(hms::is_valid_calendar_date("2000-02-29"));
    CHECK(!hms::is_valid_calendar_date("1900-02-29"));

    CHECK(hms::book_appointment(
        repo, "PID-00000001", 1, monday, "09:15", test_now()).http_status == 400);
    CHECK(hms::book_appointment(
        repo, "PID-00000001", 1, monday, "13:00", test_now()).http_status == 400);

    auto unknown_doctor = hms::book_appointment(
        repo, "PID-00000001", 99, monday, "11:00", test_now());

    CHECK(!unknown_doctor.ok);
    CHECK(unknown_doctor.http_status == 404);
    CHECK(unknown_doctor.code == "NOT_FOUND");

    // Duplicate booking returns three later free slots (same-day "HH:MM").
    CHECK(dup.code == "SLOT_TAKEN");
    CHECK(dup.message == "This slot is already booked.");
    CHECK(dup.next_slots.size() == 3);
    CHECK(dup.next_slots[0] == "10:30");
    CHECK(dup.next_slots[1] == "11:00");
    CHECK(dup.next_slots[2] == "11:30");

    const auto parsed_error =
        nlohmann::json::parse(hms::booking_error_to_json(dup, "req-1"));
    CHECK(parsed_error["error"]["code"] == "SLOT_TAKEN");
    CHECK(parsed_error["error"]["nextSlots"].size() == 3);
    CHECK(parsed_error["requestId"] == "req-1");

    CHECK(hms::book_appointment(
        repo, "PID-99999999", 1, monday, "11:00", test_now()).field == "patientId");

    CHECK(hms::slots_to_json({"09:00", "09:30"})
          == "{\"slots\":[\"09:00\",\"09:30\"]}");
    CHECK(hms::appointment_to_json(r.appointment).find(
          "\"status\":\"Confirmed\"") != std::string::npos);

    test_postgres_repository_when_enabled();

    std::cout << "All appointment tests passed.\n";
    return 0;
}
