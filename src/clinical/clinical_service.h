#pragma once

#include <string>

namespace hms::clinical {

struct VisitInput {
    long long appointment_id = 0;
    std::string symptoms;
    std::string diagnosis;
    std::string notes;
};

struct PrescriptionInput {
    long long visit_id = 0;
    std::string item;
    std::string dosage;
    std::string frequency;
    int days = 0;
};

enum class VisitRecordStatus {
    Created,
    AppointmentUnavailable,
    AlreadyRecorded
};

struct VisitRecordResult {
    VisitRecordStatus status = VisitRecordStatus::AppointmentUnavailable;
    long long visit_id = 0;
};

class ClinicalRepository {
public:
    virtual ~ClinicalRepository() = default;
    virtual VisitRecordResult record_visit(
        const VisitInput& input, const std::string& doctor_user_id) = 0;
    virtual bool add_prescription(
        const PrescriptionInput& input,
        const std::string& doctor_user_id) = 0;
};

class ClinicalService {
public:
    explicit ClinicalService(ClinicalRepository& repository);

    VisitRecordResult record_visit(
        const VisitInput& input, const std::string& doctor_user_id);
    bool add_prescription(
        const PrescriptionInput& input, const std::string& doctor_user_id);

private:
    ClinicalRepository& repository_;
};

class PostgresClinicalRepository final : public ClinicalRepository {
public:
    explicit PostgresClinicalRepository(std::string encryption_key,
                                        const char* connection_info = nullptr);
    ~PostgresClinicalRepository() override;

    PostgresClinicalRepository(const PostgresClinicalRepository&) = delete;
    PostgresClinicalRepository& operator=(const PostgresClinicalRepository&) =
        delete;

    VisitRecordResult record_visit(
        const VisitInput& input, const std::string& doctor_user_id) override;
    bool add_prescription(
        const PrescriptionInput& input,
        const std::string& doctor_user_id) override;

private:
    struct Connection;
    Connection* connection_;
    std::string encryption_key_;
};

} // namespace hms::clinical
