#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include "appointments/appointments.h"

using namespace hms;

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        ++failures; \
        std::cerr << "FAIL line " << __LINE__ \
                  << ": " #condition "\n"; \
    } \
} while (false)

static std::chrono::system_clock::time_point at(
    int day, int hour, int minute) {
    std::tm value{};
    value.tm_year = 126;  // 2026
    value.tm_mon = 9;     // October
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_isdst = -1;

    return std::chrono::system_clock::from_time_t(
        std::mktime(&value));
}

static const auto yes =
    [](const std::string&) { return true; };

static const auto no =
    [](const std::string&) { return false; };

static long long seed(InMemoryAppointmentRepository& repo) {
    const auto result = repo.insert({
        0, "PID-00000001", 1,
        "2026-10-26 10:00", "Confirmed"
    });

    return result.appointment.id;
}

static bool is_free(
    const InMemoryAppointmentRepository& repo,
    const std::string& date,
    const std::string& time) {
    const auto slots = free_slots(repo, 1, date);
    return std::find(slots.begin(), slots.end(), time) != slots.end();
}

int main() {
    // Cancel three hours before: success and slot becomes free.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto result = change_appointment(
            repo, id, "cancel", "", "", yes, at(26, 7, 0));

        CHECK(result.ok);
        CHECK(result.appointment.status == "Cancelled");
        CHECK(is_free(repo, "2026-10-26", "10:00"));
    }

    // Cancel one hour before: conflict and no change.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto result = change_appointment(
            repo, id, "cancel", "", "", yes, at(26, 9, 0));

        CHECK(result.http_status == 409);
        CHECK(result.code == "TOO_LATE_TO_CHANGE");
        CHECK(result.message == "Cannot change within 2 hours");

        Appointment appointment;
        CHECK(repo.find_by_id(id, appointment));
        CHECK(appointment.status == "Confirmed");
    }

    // Exactly two hours is allowed.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto result = change_appointment(
            repo, id, "cancel", "", "", yes, at(26, 8, 0));

        CHECK(result.ok);
    }

    // One minute under two hours is refused.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto result = change_appointment(
            repo, id, "cancel", "", "", yes, at(26, 8, 1));

        CHECK(!result.ok);
        CHECK(result.code == "TOO_LATE_TO_CHANGE");
    }

    // Reschedule to a free slot.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto result = change_appointment(
            repo, id, "reschedule",
            "2026-10-28", "10:00", yes, at(26, 7, 0));

        CHECK(result.ok);
        CHECK(result.appointment.slot_start == "2026-10-28 10:00");
        CHECK(is_free(repo, "2026-10-26", "10:00"));
        CHECK(!is_free(repo, "2026-10-28", "10:00"));
    }

    // Reschedule too late.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto result = change_appointment(
            repo, id, "reschedule",
            "2026-10-28", "10:00", yes, at(26, 9, 0));

        CHECK(!result.ok);
        CHECK(result.code == "TOO_LATE_TO_CHANGE");
    }

    // Taken slot: original appointment remains unchanged.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        repo.insert({
            0, "PID-00000002", 1,
            "2026-10-28 10:00", "Confirmed"
        });

        const auto result = change_appointment(
            repo, id, "reschedule",
            "2026-10-28", "10:00", yes, at(26, 7, 0));

        CHECK(!result.ok);
        CHECK(result.http_status == 409);
        CHECK(result.code == "SLOT_TAKEN");
        CHECK(result.suggested_slots.size() == 3);

        Appointment appointment;
        CHECK(repo.find_by_id(id, appointment));
        CHECK(appointment.slot_start == "2026-10-26 10:00");
    }

    // Reject past times and day 61.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto past = change_appointment(
            repo, id, "reschedule",
            "2026-10-26", "07:30", yes, at(26, 8, 0));

        CHECK(past.http_status == 400);
        CHECK(past.code == "BOOKING_IN_PAST");

        const auto too_far = change_appointment(
            repo, id, "reschedule",
            "2026-12-26", "10:00", yes, at(26, 7, 0));

        CHECK(too_far.http_status == 400);
        CHECK(too_far.code == "BOOKING_TOO_FAR");
    }

    // Wrong patient, unknown ID, and invalid action.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        CHECK(change_appointment(
            repo, id, "cancel", "", "", no, at(26, 7, 0))
            .http_status == 403);

        CHECK(change_appointment(
            repo, 999, "cancel", "", "", yes, at(26, 7, 0))
            .http_status == 404);

        CHECK(change_appointment(
            repo, id, "delete", "", "", yes, at(26, 7, 0))
            .http_status == 400);
    }

    // A cancelled appointment cannot be changed again.
    {
        InMemoryAppointmentRepository repo;
        const auto id = seed(repo);

        const auto first = change_appointment(
            repo, id, "cancel", "", "", yes, at(26, 7, 0));

        CHECK(first.ok);

        const auto second = change_appointment(
            repo, id, "cancel", "", "", yes, at(26, 7, 0));

        CHECK(second.http_status == 409);
        CHECK(second.code == "NOT_CHANGEABLE");
    }

    if (failures == 0) {
        std::cout << "All change_appointment tests passed\n";
    }

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}