
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
        repo, "PID-00000001", 1, monday, "09:30");
    CHECK(r.ok);
    CHECK(r.http_status == 201);
    CHECK(r.appointment.status == "Confirmed");
    CHECK(r.appointment.id == 1);

    auto r2 = hms::book_appointment(
        repo, "PID-00000002", 1, monday, "10:00");
    CHECK(r2.ok);
    CHECK(r2.appointment.id != r.appointment.id);

    auto dup = hms::book_appointment(
        repo, "PID-00000002", 1, monday, "09:30");
    CHECK(!dup.ok);
    CHECK(dup.http_status == 409);

    // Required-field and general validation.
    CHECK(hms::book_appointment(repo, "", 1, monday, "11:00").field
          == "patientId");

    auto bad_format = hms::book_appointment(
        repo, "PID-00000001", 1, "26-10-2026", "11:00");
    CHECK(!bad_format.ok);
    CHECK(bad_format.http_status == 400);
    CHECK(bad_format.code == "INVALID_DATE");

    auto impossible_date = hms::book_appointment(
        repo, "PID-00000001", 1, "2026-02-31", "11:00");
    CHECK(!impossible_date.ok);
    CHECK(impossible_date.code == "INVALID_DATE");

    auto past = hms::book_appointment(
        repo, "PID-00000001", 1, date_offset(-1), "11:00");
    CHECK(!past.ok);
    CHECK(past.http_status == 400);
    CHECK(past.code == "INVALID_DATE");

    auto too_far = hms::book_appointment(
        repo, "PID-00000001", 1, date_offset(61), "11:00");
    CHECK(!too_far.ok);
    CHECK(too_far.http_status == 400);
    CHECK(too_far.code == "INVALID_DATE");

    // Day 60: test that the date boundary itself is allowed when
    // the chosen time is a valid future slot for the doctor.
    std::string day60 = date_offset(60);
    if (weekday(day60) == 1) {
        auto boundary = hms::book_appointment(
            repo, "PID-00000001", 1, day60, "09:00");
        CHECK(boundary.ok);
    } else if (weekday(day60) == 3) {
        auto boundary = hms::book_appointment(
            repo, "PID-00000001", 1, day60, "09:00");
        CHECK(boundary.ok);
    } else if (weekday(day60) == 5) {
        auto boundary = hms::book_appointment(
            repo, "PID-00000001", 1, day60, "14:00");
        CHECK(boundary.ok);
    } else {
        // The repository's doctor has no hours on this weekday.
        // The date-range check is still exercised by the day-61 case.
    }

    CHECK(hms::book_appointment(
        repo, "PID-00000001", 1, monday, "09:15").http_status == 400);
    CHECK(hms::book_appointment(
        repo, "PID-00000001", 1, monday, "13:00").http_status == 400);
    CHECK(hms::book_appointment(
        repo, "PID-00000001", 99, monday, "11:00").http_status == 400
          || hms::book_appointment(
        repo, "PID-00000001", 99, monday, "11:00").http_status == 404);

    CHECK(hms::slots_to_json({"09:00", "09:30"})
          == "{\"slots\":[\"09:00\",\"09:30\"]}");
    CHECK(hms::appointment_to_json(r.appointment).find(
          "\"status\":\"Confirmed\"") != std::string::npos);

    std::cout << "All appointment tests passed.\n";
    return 0;
}
