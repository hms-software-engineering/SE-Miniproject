
#include "../src/appointments/appointments.h"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#define CHECK(c) do { \
    if (!(c)) { \
        std::cerr << "FAILED: " #c " (line " << __LINE__ << ")\n"; \
        std::exit(1); \
    } \
} while (0)

static std::string date_offset(int days) {
    std::time_t now = std::time(nullptr);
    std::tm local = *std::localtime(&now);
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

class BoundaryTestRepository : public hms::AppointmentRepository {
public:
    std::string working_hours(long long doctor_id) const override {
        if (doctor_id != 1) return "";
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

}

int main() {
    hms::InMemoryAppointmentRepository repo;

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

private:
    long long next_id_ = 1;
};

int main() {
    hms::InMemoryAppointmentRepository repo;

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

    CHECK(hms::slots_to_json({"09:00", "09:30"})
          == "{\"slots\":[\"09:00\",\"09:30\"]}");
    CHECK(hms::appointment_to_json(r.appointment).find(
          "\"status\":\"Confirmed\"") != std::string::npos);

    std::cout << "All appointment tests passed.\n";
    return 0;
}
