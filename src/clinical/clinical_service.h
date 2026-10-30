#pragma once

#include <string>
#include <vector>

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
    std::string override_reason;
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

enum class PrescriptionStatus {
    Created,
    VisitUnavailable,
    AllergyWarning
};

struct PrescriptionResult {
    PrescriptionStatus status = PrescriptionStatus::VisitUnavailable;
    std::vector<std::string> allergy_conflicts;
    bool overridden = false;
};

struct VisitAllergies {
    bool visit_found = false;
    // False when the stored record cannot be decrypted. It is never read as
    // "no allergies".
    bool readable = true;
    std::string allergies;
};

struct PrescriptionRecord {
    std::string item;
    std::string dosage;
    std::string frequency;
    int days = 0;
    std::string override_reason;
};

struct VisitHistoryEntry {
    long long visit_id = 0;
    std::string visited_at;
    std::string symptoms;
    std::string diagnosis;
    std::string notes;
    std::vector<PrescriptionRecord> prescriptions;
};

class ClinicalRepository {
public:
    virtual ~ClinicalRepository() = default;
    virtual VisitRecordResult record_visit(
        const VisitInput& input, const std::string& doctor_user_id) = 0;
    virtual bool add_prescription(
        const PrescriptionInput& input,
        const std::string& doctor_user_id) = 0;
    virtual VisitAllergies visit_allergies(
        long long visit_id, const std::string& doctor_user_id) = 0;
    virtual bool set_patient_allergies(
        const std::string& patient_id, const std::string& allergies) = 0;
    // Newest visit first.
    virtual std::vector<VisitHistoryEntry> visit_history(
        const std::string& patient_id) = 0;
};

// Allergies from a comma or semicolon separated list that match a medicine,
// directly or through its drug family.
std::vector<std::string> find_allergy_conflicts(
    const std::string& allergies, const std::string& medicine);

class ClinicalService {
public:
    explicit ClinicalService(ClinicalRepository& repository);

    VisitRecordResult record_visit(
        const VisitInput& input, const std::string& doctor_user_id);
    PrescriptionResult add_prescription(
        const PrescriptionInput& input, const std::string& doctor_user_id);
    bool set_patient_allergies(
        const std::string& patient_id, const std::string& allergies);
    std::vector<VisitHistoryEntry> visit_history(
        const std::string& patient_id);

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
    VisitAllergies visit_allergies(
        long long visit_id, const std::string& doctor_user_id) override;
    bool set_patient_allergies(
        const std::string& patient_id, const std::string& allergies) override;
    std::vector<VisitHistoryEntry> visit_history(
        const std::string& patient_id) override;

private:
    struct Connection;
    Connection* connection_;
    std::string encryption_key_;
};

} // namespace hms::clinical
