#include "../src/appointments/appointments.h"
#include <cstdlib>
#include <iostream>

#define CHECK(c) do { if (!(c)) { \
    std::cerr << "FAILED: " #c " line " << __LINE__ << "\n"; std::exit(1); } } while (0)

int main() {
    hms::InMemoryAppointmentRepository repo;
    CHECK(hms::free_slots(repo, "D1", "2026-10-26").size() == 16);

    auto r = hms::book_appointment(repo, "PID-00000001", "D1", "2026-10-26", "09:30");
    CHECK(r.ok && r.http_status == 201);
    CHECK(r.appointment.status == "Confirmed");
    CHECK(r.appointment.id.rfind("APT-", 0) == 0);

    // slot no longer free (TC-APT-01)
    CHECK(hms::free_slots(repo, "D1", "2026-10-26").size() == 15);

    // second booking refused
    CHECK(hms::book_appointment(repo, "PID-00000002", "D1", "2026-10-26", "09:30").http_status == 409);

    // validation
    CHECK(hms::book_appointment(repo, "", "D1", "2026-10-26", "09:30").field == "patientId");
    CHECK(hms::book_appointment(repo, "P", "D1", "2026-10-26", "09:15").http_status == 400);

    // ids are unique
    auto r2 = hms::book_appointment(repo, "P", "D1", "2026-10-26", "10:00");
    CHECK(r2.appointment.id != r.appointment.id);

    std::cout << "All appointment tests passed.\n";
    return 0;
}
