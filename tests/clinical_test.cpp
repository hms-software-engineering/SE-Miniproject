#include "../src/clinical/clinical.h"

#include <cstdlib>
#include <iostream>

#define CHECK(c)                                                          \
    do {                                                                  \
        if (!(c)) {                                                       \
            std::cerr << "FAILED: " #c " (line " << __LINE__ << ")\n";    \
            std::exit(1);                                                 \
        }                                                                 \
    } while (0)

int main() {
    hms::InMemoryClinicalRepository repo;
    repo.add_appointment({1, "PID-00000001", 7, "Confirmed"});
    repo.add_appointment({2, "PID-00000001", 7, "Confirmed"});
    repo.add_appointment({3, "PID-00000001", 7, "Confirmed"});
    repo.add_appointment({4, "PID-00000002", 7, "Confirmed"});
    repo.add_appointment({5, "PID-00000002", 8, "Confirmed"});
    repo.add_appointment({6, "PID-00000002", 7, "Cancelled"});

    hms::Caller doctor{hms::Role::Doctor, 7};
    hms::Caller receptionist{hms::Role::Receptionist, 0};

    // TC-CLN-01: visit saved with doctor ID and timestamp, linked to the appointment.
    auto v1 = hms::record_visit(repo, doctor, 1, "fever", "viral infection", "rest",
                                "2026-10-20 09:30");
    CHECK(v1.ok);
    CHECK(v1.http_status == 201);
    CHECK(v1.visit.doctor_id == 7);
    CHECK(v1.visit.appointment_id == 1);
    CHECK(v1.visit.patient_id == "PID-00000001");
    CHECK(v1.visit.visited_at == "2026-10-20 09:30");

    // One visit per appointment; validation; ownership; RBAC.
    CHECK(hms::record_visit(repo, doctor, 1, "x", "y", "", "2026-10-20 10:00").http_status == 409);
    CHECK(hms::record_visit(repo, doctor, 2, " ", "y", "", "t").field == "symptoms");
    CHECK(hms::record_visit(repo, doctor, 2, "x", "", "", "t").field == "diagnosis");
    CHECK(hms::record_visit(repo, doctor, 99, "x", "y", "", "t").http_status == 404);
    CHECK(hms::record_visit(repo, doctor, 5, "x", "y", "", "t").http_status == 403);
    CHECK(hms::record_visit(repo, doctor, 6, "x", "y", "", "t").http_status == 400);
    auto denied = hms::record_visit(repo, receptionist, 2, "x", "y", "", "t");
    CHECK(denied.http_status == 403);
    CHECK(denied.message == "Access denied");

    // TC-CLN-02: prescription item saved on the visit.
    auto p1 = hms::add_prescription(repo, doctor, v1.visit.id, "Paracetamol 500 mg",
                                    "500 mg", "3 times a day", 5);
    CHECK(p1.ok);
    CHECK(p1.http_status == 201);
    CHECK(p1.item.days == 5);
    CHECK(repo.prescriptions_for_visit(v1.visit.id).size() == 1);
    CHECK(hms::add_prescription(repo, doctor, v1.visit.id, "X", "1", "daily", 0).field == "days");
    CHECK(hms::add_prescription(repo, doctor, v1.visit.id, "", "1", "daily", 3).field == "medicine");
    CHECK(hms::add_prescription(repo, doctor, 99, "X", "1", "daily", 3).http_status == 404);
    CHECK(hms::add_prescription(repo, receptionist, v1.visit.id, "X", "1", "daily", 3).http_status == 403);

    std::cout << "All clinical tests passed.\n";
    return 0;
}
