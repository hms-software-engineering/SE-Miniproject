#include "patients/patient_service.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

#define CHECK(condition)                                                     \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << "FAILED: " #condition << " (line " << __LINE__      \
                      << ")\n";                                               \
            std::exit(1);                                                     \
        }                                                                     \
    } while (false)

namespace {

using namespace hms::patients;

struct StoredPatient {
    Patient patient;
    std::vector<BlindIndex> name_prefix_indexes;
    BlindIndex phone_index;
};

class InMemoryPatientRepository final : public PatientRepository {
public:
    Patient create_patient(
        const Registration& registration,
        const std::vector<BlindIndex>& name_prefix_indexes,
        const BlindIndex& phone_index) override
    {
        Patient patient{"PID-0000000" + std::to_string(next_id_++),
                        registration};
        items_.push_back({patient, name_prefix_indexes, phone_index});
        return patient;
    }

    bool update_patient(
        const std::string& patient_id,
        const Registration& registration,
        const std::vector<BlindIndex>& name_prefix_indexes,
        const BlindIndex& phone_index,
        const std::string&) override
    {
        for (auto& item : items_) {
            if (item.patient.patient_id == patient_id) {
                item.patient.demographics = registration;
                item.name_prefix_indexes = name_prefix_indexes;
                item.phone_index = phone_index;
                return true;
            }
        }
        return false;
    }

    std::vector<Patient> find_by_id(const std::string& patient_id) const override
    {
        std::vector<Patient> matches;
        for (const auto& item : items_) {
            if (item.patient.patient_id == patient_id) {
                matches.push_back(item.patient);
            }
        }
        return matches;
    }

    std::vector<Patient> find_by_name_prefix(
        const BlindIndex& name_prefix_index) const override
    {
        std::vector<Patient> matches;
        for (const auto& item : items_) {
            if (std::find(item.name_prefix_indexes.begin(),
                          item.name_prefix_indexes.end(),
                          name_prefix_index) !=
                item.name_prefix_indexes.end()) {
                matches.push_back(item.patient);
            }
        }
        return matches;
    }

    std::vector<Patient> find_by_phone_index(
        const BlindIndex& phone_index) const override
    {
        std::vector<Patient> matches;
        for (const auto& item : items_) {
            if (item.phone_index == phone_index) {
                matches.push_back(item.patient);
            }
        }
        return matches;
    }

    std::size_t size() const
    {
        return items_.size();
    }

private:
    std::size_t next_id_ = 1;
    std::vector<StoredPatient> items_;
};

Registration valid_registration()
{
    return {"Asha Nair", "14-03-1990", "Female", "9876543210",
            "12 MG Road, Bengaluru"};
}

std::string to_hex(const BlindIndex& index)
{
    std::ostringstream result;
    result << std::hex;
    for (unsigned char byte : index) {
        result.width(2);
        result.fill('0');
        result << static_cast<unsigned int>(byte);
    }
    return result.str();
}

void test_postgres_repository_when_enabled()
{
    const char* enabled = std::getenv("HMS_PATIENT_TEST_POSTGRES");
    if (enabled == nullptr || std::string(enabled) != "1") {
        return;
    }
    const char* data_key = std::getenv("HMS_PATIENT_DATA_KEY");
    const char* name_key = std::getenv("HMS_PATIENT_NAME_INDEX_KEY");
    const char* phone_key = std::getenv("HMS_PATIENT_PHONE_INDEX_KEY");
    CHECK(data_key != nullptr);
    CHECK(name_key != nullptr);
    CHECK(phone_key != nullptr);

    PostgresPatientRepository repository(data_key);
    PatientService service(repository, name_key, phone_key);
    const auto stamp = std::chrono::high_resolution_clock::now()
                           .time_since_epoch()
                           .count();
    const std::string unique_name =
        "Hms Patient Test " + std::to_string(stamp % 1000000000);
    const std::string phone =
        "9" + std::to_string(100000000 + stamp % 899999999);
    Registration registration{
        unique_name, "14-03-1990", "Female", phone,
        "Integration Test Address"
    };
    const auto created =
        service.register_patient(registration, "21-10-2026");
    CHECK(created.saved);
    CHECK(created.patient.patient_id.size() == 12);

    const auto by_id = service.search_patients(created.patient.patient_id);
    CHECK(by_id.matches.size() == 1);
    CHECK(by_id.matches.front().demographics.name == unique_name);
    CHECK(service.search_patients(unique_name).matches.size() == 1);
    CHECK(service.search_patients(phone).matches.size() == 1);

    registration.name += " Updated";
    registration.phone = "8" + std::to_string(100000000 + stamp % 899999999);
    const auto updated = service.update_patient(
        created.patient.patient_id, registration, "21-10-2026", "1");
    CHECK(updated.saved);

    PostgresPatientRepository second_connection(data_key);
    PatientService persisted(second_connection, name_key, phone_key);
    const auto after_update =
        persisted.search_patients(created.patient.patient_id);
    CHECK(after_update.matches.size() == 1);
    CHECK(after_update.matches.front().demographics.name == registration.name);
    CHECK(persisted.search_patients(phone).matches.empty());
    CHECK(persisted.search_patients(registration.phone).matches.size() == 1);
}

} // namespace

int main()
{
    const std::string name_key(32, 'n');
    const std::string phone_key(32, 'p');
    InMemoryPatientRepository repository;
    PatientService service(repository, name_key, phone_key);

    const auto first = service.register_patient(
        valid_registration(), "21-10-2026");
    CHECK(first.saved);
    CHECK(first.errors.empty());
    CHECK(first.patient.patient_id == "PID-00000001");
    CHECK(first.patient.patient_id.size() == 12);

    Registration second_registration = valid_registration();
    second_registration.name = "Ashwin Rao";
    second_registration.phone = "9876543211";
    const auto second = service.register_patient(
        second_registration, "21-10-2026");
    CHECK(second.saved);
    CHECK(second.patient.patient_id == "PID-00000002");

    auto by_id = service.search_patients("PID-00000001");
    CHECK(by_id.matches.size() == 1);
    CHECK(by_id.matches.front().patient_id == first.patient.patient_id);

    auto by_prefix = service.search_patients("  aSH  ");
    CHECK(by_prefix.matches.size() == 2);
    CHECK(by_prefix.message.empty());

    auto by_phone = service.search_patients("9876543210");
    CHECK(by_phone.matches.size() == 1);
    CHECK(by_phone.matches.front().patient_id == first.patient.patient_id);

    Registration updated_registration = valid_registration();
    updated_registration.name = "Asha Menon";
    updated_registration.phone = "9876543212";
    const auto updated = service.update_patient(
        first.patient.patient_id, updated_registration, "21-10-2026", "8");
    CHECK(updated.saved);
    CHECK(updated.patient.demographics.name == "Asha Menon");
    CHECK(service.search_patients("Asha").matches.size() == 1);
    CHECK(service.search_patients("9876543210").matches.empty());
    CHECK(service.search_patients("9876543212").matches.size() == 1);

    const auto missing = service.update_patient(
        "PID-00000099", updated_registration, "21-10-2026", "8");
    CHECK(!missing.saved);
    CHECK(missing.errors.front().field == "patientId");

    auto no_match = service.search_patients("PID-00000000");
    CHECK(no_match.matches.empty());
    CHECK(no_match.message == "No patient found");
    CHECK(service.search_patients("   ").message == "Search query is required.");

    const auto reject = [&service, &repository](Registration registration) {
        const std::size_t count_before = repository.size();
        const auto result =
            service.register_patient(registration, "21-10-2026");
        CHECK(!result.saved);
        CHECK(!result.errors.empty());
        CHECK(repository.size() == count_before);
        return result.errors.front().field;
    };

    Registration invalid = valid_registration();
    invalid.name = "  ";
    CHECK(reject(invalid) == "name");

    invalid = valid_registration();
    invalid.date_of_birth = "31-02-1990";
    CHECK(reject(invalid) == "dateOfBirth");

    invalid = valid_registration();
    invalid.date_of_birth = "01-01-2030";
    CHECK(reject(invalid) == "dateOfBirth");

    invalid = valid_registration();
    invalid.gender.clear();
    CHECK(reject(invalid) == "gender");

    invalid = valid_registration();
    invalid.phone.clear();
    CHECK(reject(invalid) == "phone");

    invalid = valid_registration();
    invalid.phone = "123456789";
    CHECK(reject(invalid) == "phone");

    invalid = valid_registration();
    invalid.phone = "12345678901";
    CHECK(reject(invalid) == "phone");

    invalid = valid_registration();
    invalid.address.clear();
    CHECK(reject(invalid) == "address");

    bool rejected_short_key = false;
    try {
        PatientService invalid_service(repository, "short", phone_key);
    } catch (const std::invalid_argument&) {
        rejected_short_key = true;
    }
    CHECK(rejected_short_key);

    CHECK(make_name_prefix_blind_index("Ash", name_key) ==
          make_name_prefix_blind_index("  aSH ", name_key));
    CHECK(make_name_prefix_blind_index("Ash", name_key) !=
          make_phone_blind_index("Ash", name_key));
    CHECK(to_hex(make_name_prefix_blind_index("Ash", name_key)) ==
          "21c0c2ce4bf33e77a406de6523fc1a2d5895291cdf52a82a823d4e89f039f57e");
    CHECK(to_hex(make_phone_blind_index("9876543210", phone_key)) ==
          "3da1c78c823b2c6d43e879923c8968893b46b77a06f7b93b15f5a322313243da");

    test_postgres_repository_when_enabled();

    std::cout << "All patient service tests passed.\n";
    return 0;
}
