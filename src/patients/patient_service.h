#pragma once

#include <array>
#include <string>
#include <vector>

namespace hms::patients {

inline constexpr std::size_t BLIND_INDEX_SIZE = 32;
using BlindIndex = std::array<unsigned char, BLIND_INDEX_SIZE>;

struct Registration {
    std::string name;
    std::string date_of_birth;
    std::string gender;
    std::string phone;
    std::string address;
    std::string linked_user_id;
    std::string allergies;
};

struct Patient {
    std::string patient_id;
    Registration demographics;
    std::string owner_user_id;
};

struct PatientHistoryEntry {
    Registration old_values;
    std::string changed_at;
    std::string changed_by;
};

struct ValidationError {
    std::string field;
    std::string message;
};

struct RegistrationResult {
    bool saved = false;
    bool duplicate_warning = false;
    Patient patient;
    std::vector<ValidationError> errors;
};

struct SearchResult {
    std::vector<Patient> matches;
    std::string message;
};

class PatientRepository {
public:
    virtual ~PatientRepository() = default;

    virtual Patient create_patient(
        const Registration& registration,
        const std::vector<BlindIndex>& name_prefix_indexes,
        const BlindIndex& phone_index) = 0;
    virtual bool update_patient(
        const std::string& patient_id,
        const Registration& registration,
        const std::vector<BlindIndex>& name_prefix_indexes,
        const BlindIndex& phone_index,
        const std::string& changed_by_user_id) = 0;

    virtual std::vector<Patient> find_by_id(const std::string& patient_id) const = 0;
    virtual std::vector<Patient> find_by_name_prefix(
        const BlindIndex& name_prefix_index) const = 0;
    virtual std::vector<Patient> find_by_phone_index(
        const BlindIndex& phone_index) const = 0;
    virtual std::vector<PatientHistoryEntry> patient_history(
        const std::string& patient_id) const = 0;
};

class PatientService {
public:
    PatientService(PatientRepository& repository,
                   std::string name_index_key,
                   std::string phone_index_key);

    RegistrationResult register_patient(const Registration& registration,
                                        const std::string& today,
                                        bool confirm_duplicate = false) const;
    RegistrationResult update_patient(const std::string& patient_id,
                                     const Registration& registration,
                                     const std::string& today,
                                     const std::string& changed_by_user_id) const;
    std::vector<PatientHistoryEntry> patient_history(
        const std::string& patient_id) const;
    SearchResult search_patients(const std::string& query) const;

private:
    PatientRepository& repository_;
    std::string name_index_key_;
    std::string phone_index_key_;
};

std::string normalize_name_search(const std::string& value);
BlindIndex make_name_prefix_blind_index(const std::string& prefix,
                                        const std::string& key);
std::vector<BlindIndex> make_name_prefix_blind_indexes(
    const std::string& name,
    const std::string& key);
BlindIndex make_phone_blind_index(const std::string& phone,
                                  const std::string& key);

class PostgresPatientRepository final : public PatientRepository {
public:
    explicit PostgresPatientRepository(
        std::string data_encryption_key,
        const char* connection_info = nullptr);
    ~PostgresPatientRepository() override;

    PostgresPatientRepository(const PostgresPatientRepository&) = delete;
    PostgresPatientRepository& operator=(
        const PostgresPatientRepository&) = delete;

    Patient create_patient(
        const Registration& registration,
        const std::vector<BlindIndex>& name_prefix_indexes,
        const BlindIndex& phone_index) override;
    bool update_patient(
        const std::string& patient_id,
        const Registration& registration,
        const std::vector<BlindIndex>& name_prefix_indexes,
        const BlindIndex& phone_index,
        const std::string& changed_by_user_id) override;
    std::vector<Patient> find_by_id(
        const std::string& patient_id) const override;
    std::vector<Patient> find_by_name_prefix(
        const BlindIndex& name_prefix_index) const override;
    std::vector<Patient> find_by_phone_index(
        const BlindIndex& phone_index) const override;
    std::vector<PatientHistoryEntry> patient_history(
        const std::string& patient_id) const override;

private:
    struct Connection;
    Connection* connection_;
    std::string data_encryption_key_;
};

} // namespace hms::patients
