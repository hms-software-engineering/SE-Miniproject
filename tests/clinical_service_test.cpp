#include "clinical/clinical_service.h"
#include "appointments/appointments.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
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

std::string integration_test_date()
{
    using namespace std::chrono;
    const auto half_hour =
        duration_cast<minutes>(system_clock::now().time_since_epoch()).count() /
        30;
    const sys_days base = year{2000}/January/1;
    const year_month_day date = base + days{half_hour % 10000};
    char text[11];
    std::snprintf(text, sizeof(text), "%04d-%02u-%02u",
                  int(date.year()), unsigned(date.month()), unsigned(date.day()));
    return text;
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

    test_postgres_repository_when_enabled();

    std::cout << "All clinical service tests passed.\n";
    return 0;
}
