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
    repo.set_allergies("PID-00000001", {"Penicillin"});

    hms::Caller doctor{hms::Role::Doctor, 7, ""};
    hms::Caller receptionist{hms::Role::Receptionist, 0, ""};
    hms::Caller asha{hms::Role::Patient, 0, "PID-00000001"};

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
                                    "500 mg", "3 times a day", 5, "");
    CHECK(p1.ok);
    CHECK(p1.http_status == 201);
    CHECK(p1.item.days == 5);
    CHECK(repo.prescriptions_for_visit(v1.visit.id).size() == 1);
    CHECK(hms::add_prescription(repo, doctor, v1.visit.id, "X", "1", "daily", 0, "").field == "days");
    CHECK(hms::add_prescription(repo, doctor, v1.visit.id, "", "1", "daily", 3, "").field == "medicine");
    CHECK(hms::add_prescription(repo, doctor, 99, "X", "1", "daily", 3, "").http_status == 404);
    CHECK(hms::add_prescription(repo, receptionist, v1.visit.id, "X", "1", "daily", 3, "").http_status == 403);

    // TC-CLN-03: Penicillin allergy, Amoxicillin prescribed -> warning until reason given.
    auto warn = hms::add_prescription(repo, doctor, v1.visit.id, "Amoxicillin 250 mg",
                                      "250 mg", "twice a day", 7, "");
    CHECK(!warn.ok);
    CHECK(warn.http_status == 409);
    CHECK(warn.code == "ALLERGY_WARNING");
    CHECK(repo.prescriptions_for_visit(v1.visit.id).size() == 1);
    auto over = hms::add_prescription(repo, doctor, v1.visit.id, "Amoxicillin 250 mg",
                                      "250 mg", "twice a day", 7, "No alternative; monitored");
    CHECK(over.ok);
    CHECK(over.item.override_reason == "No alternative; monitored");
    CHECK(hms::matching_allergies("PENICILLIN V", {"penicillin"}).size() == 1);
    CHECK(hms::matching_allergies("Paracetamol", {"Penicillin"}).empty());

    // TC-CLN-04: three visits on different dates listed newest first.
    auto v2 = hms::record_visit(repo, doctor, 2, "cough", "bronchitis", "", "2026-10-24 11:00");
    auto v3 = hms::record_visit(repo, doctor, 3, "headache", "migraine", "", "2026-10-22 15:30");
    CHECK(v2.ok && v3.ok);
    auto h = hms::patient_history(repo, doctor, "PID-00000001");
    CHECK(h.ok);
    CHECK(h.history.size() == 3);
    CHECK(h.history[0].visit.diagnosis == "bronchitis");
    CHECK(h.history[1].visit.diagnosis == "migraine");
    CHECK(h.history[2].visit.diagnosis == "viral infection");
    CHECK(h.history[2].prescriptions.size() == 2);
    hms::Caller billing{hms::Role::BillingOfficer, 0};
    CHECK(hms::patient_history(repo, billing, "PID-00000001").http_status == 403);

    // TC-CLN-05: patient sees only own history; another patient's returns Access denied.
    hms::record_visit(repo, doctor, 4, "rash", "dermatitis", "", "2026-10-25 09:00");
    auto own = hms::patient_history(repo, asha, "PID-00000001");
    CHECK(own.ok);
    CHECK(own.history.size() == 3);
    auto other = hms::patient_history(repo, asha, "PID-00000002");
    CHECK(other.http_status == 403);
    CHECK(other.message == "Access denied");

    // JSON output.
    auto json = hms::history_to_json(h.history);
    CHECK(json.find("\"diagnosis\":\"bronchitis\"") < json.find("\"diagnosis\":\"migraine\""));
    CHECK(json.find("\"medicine\":\"Paracetamol 500 mg\"") != std::string::npos);

    std::cout << "All clinical tests passed.\n";
    return 0;
}
