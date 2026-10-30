#include "clinical/clinical_service.h"
#include "appointments/appointments.h"
#include "test_date_utils.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(condition)                                                   \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::cerr << "FAILED: " #condition " (line " << __LINE__       \
                      << ")\n";                                             \
            std::exit(1);                                                   \
        }                                                                   \
    } while (false)

namespace {

std::string integration_test_date()
{
    return test_support::integration_test_date();
}

void test_postgres_repository_when_enabled()
{
    const char* enabled = std::getenv("HMS_CLINICAL_TEST_POSTGRES");
    if (enabled == nullptr || std::string(enabled) != "1") return;

    bool rejected_short_key = false;
    try {
        hms::clinical::PostgresClinicalRepository invalid("short");
    } catch (const std::invalid_argument&) {
        rejected_short_key = true;
    }
    CHECK(rejected_short_key);

    hms::PostgresAppointmentRepository appointments;
    const auto appointment = appointments.insert(
        {0, "PID-00000001", 1, integration_test_date() + " 09:30", {}});
    CHECK(appointment.status == hms::SaveStatus::Ok);

    hms::clinical::PostgresClinicalRepository repository(
        "0123456789abcdef0123456789abcdef");
    const hms::clinical::VisitInput input{
        appointment.appointment.id, "Cough", "Upper respiratory infection",
        "Rest and fluids"};
    CHECK(repository.record_visit(input, "3").status ==
          hms::clinical::VisitRecordStatus::AppointmentUnavailable);
    const auto created = repository.record_visit(input, "2");
    CHECK(created.status == hms::clinical::VisitRecordStatus::Created);
    CHECK(created.visit_id > 0);
    CHECK(repository.record_visit(input, "2").status ==
          hms::clinical::VisitRecordStatus::AlreadyRecorded);
    CHECK(repository.add_prescription(
        {created.visit_id, "Medicine", "1 tablet", "Twice daily", 5}, "2"));
    CHECK(!repository.add_prescription(
        {created.visit_id, "Medicine", "1 tablet", "Twice daily", 5}, "3"));

    CHECK(repository.set_patient_allergies("PID-00000001", "Penicillin"));
    CHECK(!repository.set_patient_allergies("PID-99999999", "Penicillin"));
    const auto allergies = repository.visit_allergies(created.visit_id, "2");
    CHECK(allergies.visit_found && allergies.readable);
    CHECK(allergies.allergies == "Penicillin");
    CHECK(!repository.visit_allergies(created.visit_id, "3").visit_found);

    hms::clinical::ClinicalService service(repository);
    CHECK(service.add_prescription(
        {created.visit_id, "Amoxicillin", "1 tablet", "Daily", 5}, "2")
              .status == hms::clinical::PrescriptionStatus::AllergyWarning);
    CHECK(service.add_prescription(
        {created.visit_id, "Amoxicillin", "1 tablet", "Daily", 5,
         "No alternative available"}, "2").overridden);

    hms::clinical::PostgresClinicalRepository wrong_key(
        "fedcba9876543210fedcba9876543210");
    const auto unreadable = wrong_key.visit_allergies(created.visit_id, "2");
    CHECK(unreadable.visit_found && !unreadable.readable);
    CHECK(repository.set_patient_allergies("PID-00000001", ""));

    // TC-CLN-04: the later visit comes first, with its prescriptions.
    const auto later_appointment = appointments.insert(
        {0, "PID-00000001", 1, integration_test_date() + " 10:30", {}});
    CHECK(later_appointment.status == hms::SaveStatus::Ok);
    const auto later = repository.record_visit(
        {later_appointment.appointment.id, "Fever", "Viral fever", ""}, "2");
    CHECK(later.status == hms::clinical::VisitRecordStatus::Created);
    CHECK(repository.add_prescription(
        {later.visit_id, "Paracetamol", "1 tablet", "Daily", 3}, "2"));

    const auto history = repository.visit_history("PID-00000001");
    CHECK(history.size() >= 2);
    CHECK(history[0].visit_id == later.visit_id);
    CHECK(history[0].visited_at >= history[1].visited_at);
    CHECK(history[0].symptoms == "Fever");
    CHECK(history[0].diagnosis == "Viral fever");
    CHECK(history[0].prescriptions.size() == 1);
    CHECK(history[0].prescriptions[0].item == "Paracetamol");
    CHECK(history[0].prescriptions[0].days == 3);
    CHECK(history[1].visit_id == created.visit_id);
    CHECK(history[1].notes == "Rest and fluids");
    CHECK(history[1].prescriptions.size() == 2);
    CHECK(history[1].prescriptions[1].override_reason ==
          "No alternative available");
    CHECK(repository.visit_history("PID-99999999").empty());
}

class InMemoryClinicalRepository final : public hms::clinical::ClinicalRepository {
public:
    hms::clinical::VisitRecordResult record_visit(
        const hms::clinical::VisitInput& input,
        const std::string& doctor_user_id) override
    {
        if (doctor_user_id != "doctor-1" || input.appointment_id != 7) {
            return {hms::clinical::VisitRecordStatus::AppointmentUnavailable,
                    0};
        }
        if (visit_created_) {
            return {hms::clinical::VisitRecordStatus::AlreadyRecorded, 0};
        }
        visit_created_ = true;
        return {hms::clinical::VisitRecordStatus::Created, 12};
    }

    bool add_prescription(
        const hms::clinical::PrescriptionInput& input,
        const std::string& doctor_user_id) override
    {
        if (!visit_created_ || doctor_user_id != "doctor-1" ||
            input.visit_id != 12) {
            return false;
        }
        prescription_created_ = true;
        stored_override_reason_ = input.override_reason;
        return true;
    }

    hms::clinical::VisitAllergies visit_allergies(
        long long visit_id, const std::string& doctor_user_id) override
    {
        hms::clinical::VisitAllergies recorded;
        recorded.visit_found = visit_created_ &&
            doctor_user_id == "doctor-1" && visit_id == 12;
        recorded.readable = allergies_readable_;
        recorded.allergies = allergies_;
        return recorded;
    }

    bool set_patient_allergies(const std::string& patient_id,
                               const std::string& allergies) override
    {
        if (patient_id != "PID-00000001") return false;
        allergies_ = allergies;
        return true;
    }

    std::vector<hms::clinical::VisitHistoryEntry> visit_history(
        const std::string& patient_id) override
    {
        if (patient_id != "PID-00000001") return {};
        return {{12, "2026-10-20 10:00", "Cough", "Infection", "", {}}};
    }

    void corrupt_allergies() { allergies_readable_ = false; }
    bool prescription_created() const { return prescription_created_; }
    void reset_prescription() { prescription_created_ = false; }
    const std::string& stored_override_reason() const
    {
        return stored_override_reason_;
    }

private:
    bool visit_created_ = false;
    bool prescription_created_ = false;
    bool allergies_readable_ = true;
    std::string allergies_;
    std::string stored_override_reason_;
};

// TC-CLN-03: a matching medicine is held until a reason is given.
void test_allergy_warning(InMemoryClinicalRepository& repository,
                          hms::clinical::ClinicalService& service)
{
    using hms::clinical::PrescriptionStatus;
    using hms::clinical::find_allergy_conflicts;

    CHECK(find_allergy_conflicts("Penicillin, Dust", "Amoxicillin 500mg") ==
          std::vector<std::string>{"Penicillin"});
    CHECK(find_allergy_conflicts("Aspirin", "ASPIRIN 75mg").size() == 1);
    CHECK(find_allergy_conflicts("nsaid; sulfa", "Ibuprofen").size() == 1);
    CHECK(find_allergy_conflicts("Penicillin", "Paracetamol").empty());
    CHECK(find_allergy_conflicts("", "Paracetamol").empty());
    CHECK(find_allergy_conflicts(" , ;", "Paracetamol").empty());

    CHECK(service.set_patient_allergies("PID-00000001", " Penicillin "));
    CHECK(!service.set_patient_allergies("PID-00000002", "Penicillin"));
    CHECK(!service.set_patient_allergies(
        "PID-00000001", std::string(2001, 'x')));

    repository.reset_prescription();
    const hms::clinical::PrescriptionInput risky{
        12, "Amoxicillin", "1 tablet", "Daily", 5};
    const auto warned = service.add_prescription(risky, "doctor-1");
    CHECK(warned.status == PrescriptionStatus::AllergyWarning);
    CHECK(warned.allergy_conflicts ==
          std::vector<std::string>{"Penicillin"});
    CHECK(!repository.prescription_created());

    auto blank_reason = risky;
    blank_reason.override_reason = "   ";
    CHECK(service.add_prescription(blank_reason, "doctor-1").status ==
          PrescriptionStatus::AllergyWarning);
    CHECK(!repository.prescription_created());

    auto reasoned = risky;
    reasoned.override_reason = "Tolerated previously";
    const auto overridden = service.add_prescription(reasoned, "doctor-1");
    CHECK(overridden.status == PrescriptionStatus::Created);
    CHECK(overridden.overridden);
    CHECK(repository.stored_override_reason() == "Tolerated previously");

    auto unrelated = reasoned;
    unrelated.item = "Paracetamol";
    const auto safe = service.add_prescription(unrelated, "doctor-1");
    CHECK(safe.status == PrescriptionStatus::Created);
    CHECK(!safe.overridden);
    CHECK(repository.stored_override_reason().empty());

    repository.corrupt_allergies();
    CHECK(service.add_prescription(
        {12, "Paracetamol", "1 tablet", "Daily", 5}, "doctor-1").status ==
          PrescriptionStatus::AllergyWarning);
}

} // namespace

int main()
{
    InMemoryClinicalRepository repository;
    hms::clinical::ClinicalService service(repository);

    const auto visit = service.record_visit(
        {7, "Cough", "Upper respiratory infection", "Rest and fluids"},
        "doctor-1");
    CHECK(visit.status == hms::clinical::VisitRecordStatus::Created);
    CHECK(visit.visit_id == 12);
    CHECK(service.record_visit(
        {0, "Cough", "Diagnosis", ""}, "doctor-1").status ==
          hms::clinical::VisitRecordStatus::AppointmentUnavailable);
    CHECK(service.record_visit(
        {7, "Cough", "Diagnosis", ""}, "another-doctor").status ==
          hms::clinical::VisitRecordStatus::AppointmentUnavailable);
    CHECK(service.record_visit(
        {7, "Cough", "Diagnosis", ""}, "doctor-1").status ==
          hms::clinical::VisitRecordStatus::AlreadyRecorded);

    CHECK(service.add_prescription(
        {12, "Medicine", "1 tablet", "Twice daily", 5}, "doctor-1").status ==
          hms::clinical::PrescriptionStatus::Created);
    CHECK(repository.prescription_created());
    CHECK(service.add_prescription(
        {12, "Medicine", "1 tablet", "Twice daily", 0}, "doctor-1").status ==
          hms::clinical::PrescriptionStatus::VisitUnavailable);
    CHECK(service.add_prescription(
        {12, "Medicine", "1 tablet", "Twice daily", 5}, "another-doctor")
              .status == hms::clinical::PrescriptionStatus::VisitUnavailable);

    test_allergy_warning(repository, service);
    CHECK(service.visit_history("PID-00000001").size() == 1);
    CHECK(service.visit_history("PID-00000002").empty());
    CHECK(service.visit_history("").empty());
    test_postgres_repository_when_enabled();

    std::cout << "All clinical service tests passed.\n";
    return 0;
}
