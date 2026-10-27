#include "clinical/clinical_service.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#define CHECK(condition)                                                   \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::cerr << "FAILED: " #condition " (line " << __LINE__       \
                      << ")\n";                                             \
            std::exit(1);                                                   \
        }                                                                   \
    } while (false)

namespace {

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
        return true;
    }

    bool prescription_created() const { return prescription_created_; }

private:
    bool visit_created_ = false;
    bool prescription_created_ = false;
};

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
        {12, "Medicine", "1 tablet", "Twice daily", 5}, "doctor-1"));
    CHECK(repository.prescription_created());
    CHECK(!service.add_prescription(
        {12, "Medicine", "1 tablet", "Twice daily", 0}, "doctor-1"));
    CHECK(!service.add_prescription(
        {12, "Medicine", "1 tablet", "Twice daily", 5}, "another-doctor"));

    std::cout << "All clinical service tests passed.\n";
    return 0;
}
