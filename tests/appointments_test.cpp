#include "../src/appointments/appointments.h"

#include <cstdlib>
#include <ctime>
#include <iostream>

#define CHECK(c)                                                          \
    do {                                                                  \
        if (!(c)) {                                                       \
            std::cerr << "FAILED: " #c " (line " << __LINE__ << ")\n";    \
            std::exit(1);                                                 \
        }                                                                 \
    } while (0)

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

    // 2026-10-26 is a Monday, 27 Tuesday, 28 Wednesday, 30 Friday.
    CHECK(hms::weekday_of("2026-10-26") == 1);

    // Monday 09:00-13:00 gives 8 half-hour slots, ending at 12:30.
    auto monday = hms::free_slots(repo, 1, "2026-10-26");
    CHECK(monday.size() == 8);
    CHECK(monday.front() == "09:00");
    CHECK(monday.back() == "12:30");

    // Doctor does not work Tuesday.
    CHECK(hms::free_slots(repo, 1, "2026-10-27").empty());
    // Friday 14:00-17:00 gives 6 slots.
    CHECK(hms::free_slots(repo, 1, "2026-10-30").size() == 6);

    // TC-APT-01: booking is stored and the slot is no longer free.
    auto r = hms::book_appointment(repo, "PID-00000001", 1,
                                   "2026-10-26", "09:30", test_now());
    CHECK(r.ok);
    CHECK(r.http_status == 201);
    CHECK(r.appointment.status == "Confirmed");
    CHECK(r.appointment.id == 1);
    CHECK(hms::free_slots(repo, 1, "2026-10-26").size() == 7);

    // Appointment IDs are unique.
    auto r2 = hms::book_appointment(repo, "PID-00000002", 1,
                                    "2026-10-26", "10:00", test_now());
    CHECK(r2.ok);
    CHECK(r2.appointment.id != r.appointment.id);

    // Same slot twice is refused.
    auto dup = hms::book_appointment(repo, "PID-00000002", 1,
                                     "2026-10-26", "09:30", test_now());
    CHECK(!dup.ok);
    CHECK(dup.http_status == 409);
    CHECK(dup.suggested_slots.size() == 3);
    CHECK(dup.suggested_slots[0] == "2026-10-26 10:30");

    // Validation.
    CHECK(hms::book_appointment(repo, "", 1, "2026-10-26", "11:00",
                                test_now()).field == "patientId");
    CHECK(hms::book_appointment(repo, "PID-00000001", 1, "26-10-2026",
                                "11:00", test_now()).field == "date");
    CHECK(hms::book_appointment(repo, "PID-00000001", 1, "2026-10-26",
                                "09:15", test_now()).http_status == 400);
    CHECK(hms::book_appointment(repo, "PID-00000001", 1, "2026-10-26",
                                "13:00", test_now()).http_status == 400);
    CHECK(hms::book_appointment(repo, "PID-00000001", 1, "2026-10-27",
                                "09:00", test_now()).http_status == 400);
    CHECK(hms::book_appointment(repo, "PID-99999999", 1, "2026-10-26",
                                "11:00", test_now()).field == "patientId");
    CHECK(hms::book_appointment(repo, "PID-00000001", 99, "2026-10-26",
                                "11:00", test_now()).http_status == 404);
    CHECK(hms::book_appointment(repo, "PID-00000001", 1, "2026-10-24",
                                "09:00", test_now()).code == "BOOKING_IN_PAST");
    CHECK(hms::book_appointment(repo, "PID-00000001", 1, "2026-12-25",
                                "09:00", test_now()).code == "BOOKING_TOO_FAR");

    // JSON output.
    CHECK(hms::slots_to_json({"09:00", "09:30"}) == "{\"slots\":[\"09:00\",\"09:30\"]}");
    CHECK(hms::appointment_to_json(r.appointment).find("\"status\":\"Confirmed\"") != std::string::npos);

    std::cout << "All appointment tests passed.\n";
    return 0;
}
