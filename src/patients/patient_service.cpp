#include "patients/patient_service.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <utility>

namespace hms::patients {
namespace {

constexpr std::size_t MINIMUM_INDEX_KEY_SIZE = 32;
constexpr char NAME_INDEX_DOMAIN[] = "hms:patient:name-prefix:v1:";
constexpr char PHONE_INDEX_DOMAIN[] = "hms:patient:phone:v1:";

void require_valid_key(const std::string& key)
{
    if (key.size() < MINIMUM_INDEX_KEY_SIZE) {
        throw std::invalid_argument(
            "Patient blind-index keys must contain at least 32 bytes.");
    }
}

BlindIndex hmac_sha256(const std::string& key, const std::string& value)
{
    require_valid_key(key);
    BlindIndex digest{};

#ifdef _WIN32
    if (key.size() > std::numeric_limits<ULONG>::max() ||
        value.size() > std::numeric_limits<ULONG>::max()) {
        throw std::length_error("Patient blind-index input is too large.");
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD result_size = 0;
    DWORD bytes_read = 0;
    std::vector<unsigned char> object;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
        BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status >= 0) {
        status = BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
            &bytes_read, 0);
    }
    if (status >= 0) {
        status = BCryptGetProperty(
            algorithm, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&result_size), sizeof(result_size),
            &bytes_read, 0);
    }
    if (status >= 0 && result_size != digest.size()) {
        status = static_cast<NTSTATUS>(-1);
    }
    if (status >= 0) {
        object.resize(object_size);
        status = BCryptCreateHash(
            algorithm, &hash, object.data(), object_size,
            reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
            static_cast<ULONG>(key.size()), 0);
    }
    if (status >= 0) {
        status = BCryptHashData(
            hash, reinterpret_cast<PUCHAR>(
                      const_cast<char*>(value.data())),
            static_cast<ULONG>(value.size()), 0);
    }
    if (status >= 0) {
        status = BCryptFinishHash(hash, digest.data(),
                                  static_cast<ULONG>(digest.size()), 0);
    }

    if (hash != nullptr) {
        BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    if (status < 0) {
        throw std::runtime_error("Could not create patient blind index.");
    }
#else
    if (key.size() > static_cast<std::size_t>(
                         std::numeric_limits<int>::max())) {
        throw std::length_error("Patient blind-index key is too large.");
    }

    unsigned int digest_size = 0;
    if (HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char*>(value.data()),
             value.size(), digest.data(), &digest_size) == nullptr ||
        digest_size != digest.size()) {
        throw std::runtime_error("Could not create patient blind index.");
    }
#endif

    return digest;
}

bool is_ascii_space(unsigned char value)
{
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

std::string trim_ascii(const std::string& value)
{
    std::size_t first = 0;
    while (first < value.size() &&
           is_ascii_space(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           is_ascii_space(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }

    return value.substr(first, last - first);
}

bool parse_date(const std::string& value, int& year, int& month, int& day)
{
    if (value.size() != 10 || value[2] != '-' || value[5] != '-') {
        return false;
    }

    const auto digits = [&value](std::size_t start, std::size_t length) {
        for (std::size_t i = start; i < start + length; ++i) {
            if (value[i] < '0' || value[i] > '9') {
                return false;
            }
        }
        return true;
    };
    if (!digits(0, 2) || !digits(3, 2) || !digits(6, 4)) {
        return false;
    }

    day = std::stoi(value.substr(0, 2));
    month = std::stoi(value.substr(3, 2));
    year = std::stoi(value.substr(6, 4));
    if (year < 1 || month < 1 || month > 12) {
        return false;
    }

    static constexpr int days_per_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    const bool leap_year =
        year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int maximum_day =
        days_per_month[month - 1] + (month == 2 && leap_year ? 1 : 0);
    return day >= 1 && day <= maximum_day;
}

bool is_valid_date(const std::string& value)
{
    int year = 0;
    int month = 0;
    int day = 0;
    return parse_date(value, year, month, day);
}

bool is_future_date(const std::string& value, const std::string& today)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int today_year = 0;
    int today_month = 0;
    int today_day = 0;
    if (!parse_date(value, year, month, day) ||
        !parse_date(today, today_year, today_month, today_day)) {
        return false;
    }

    if (year != today_year) {
        return year > today_year;
    }
    if (month != today_month) {
        return month > today_month;
    }
    return day > today_day;
}

std::vector<ValidationError> validate_registration(
    const Registration& registration,
    const std::string& today)
{
    if (!is_valid_date(today)) {
        throw std::invalid_argument("The current date must be DD-MM-YYYY.");
    }

    const auto required = [](const std::string& value) {
        return !trim_ascii(value).empty();
    };

    if (!required(registration.name)) {
        return {{"name", "Name is required."}};
    }
    if (registration.name.size() > 60) {
        return {{"name", "Name must be at most 60 characters."}};
    }
    if (!is_valid_date(registration.date_of_birth)) {
        return {{"dateOfBirth", "Date of birth must be a valid DD-MM-YYYY date."}};
    }
    if (is_future_date(registration.date_of_birth, today)) {
        return {{"dateOfBirth", "Date of birth cannot be in the future."}};
    }
    if (!required(registration.gender)) {
        return {{"gender", "Gender is required."}};
    }
    if (registration.gender != "Female" &&
        registration.gender != "Male" &&
        registration.gender != "Other") {
        return {{"gender", "Gender must be Female, Male, or Other."}};
    }
    if (registration.phone.size() != 10 ||
        !std::all_of(registration.phone.begin(), registration.phone.end(),
                     [](unsigned char digit) {
                         return digit >= '0' && digit <= '9';
                     })) {
        return {{"phone", "Phone number must contain exactly 10 digits."}};
    }
    if (!required(registration.address)) {
        return {{"address", "Address is required."}};
    }
    if (registration.address.size() > 120) {
        return {{"address", "Address must be at most 120 characters."}};
    }
    return {};
}

bool is_patient_id(const std::string& value)
{
    if (value.size() != 12 || value.compare(0, 4, "PID-") != 0) {
        return false;
    }
    return std::all_of(value.begin() + 4, value.end(),
                       [](unsigned char digit) {
                           return digit >= '0' && digit <= '9';
                       });
}

bool is_phone_query(const std::string& value)
{
    return value.size() == 10 &&
           std::all_of(value.begin(), value.end(),
                       [](unsigned char digit) {
                           return digit >= '0' && digit <= '9';
                       });
}

std::string normalize_phone(const std::string& value)
{
    return value;
}

} // namespace

std::string normalize_name_search(const std::string& value)
{
    const std::string trimmed = trim_ascii(value);
    std::string normalized;
    normalized.reserve(trimmed.size());

    bool pending_space = false;
    for (char raw_character : trimmed) {
        const auto character =
            static_cast<unsigned char>(raw_character);
        if (is_ascii_space(character)) {
            pending_space = true;
            continue;
        }
        if (pending_space && !normalized.empty()) {
            normalized.push_back(' ');
        }
        pending_space = false;
        normalized.push_back(static_cast<char>(std::tolower(character)));
    }
    return normalized;
}

BlindIndex make_name_prefix_blind_index(const std::string& prefix,
                                        const std::string& key)
{
    const std::string normalized = normalize_name_search(prefix);
    if (normalized.empty()) {
        throw std::invalid_argument("Name search prefix cannot be empty.");
    }
    return hmac_sha256(key, std::string(NAME_INDEX_DOMAIN) + normalized);
}

std::vector<BlindIndex> make_name_prefix_blind_indexes(
    const std::string& name,
    const std::string& key)
{
    const std::string normalized = normalize_name_search(name);
    std::vector<BlindIndex> indexes;
    if (normalized.empty()) {
        return indexes;
    }

    for (std::size_t end = 1; end <= normalized.size(); ++end) {
        if (end < normalized.size() &&
            (static_cast<unsigned char>(normalized[end]) & 0xC0) == 0x80) {
            continue;
        }
        indexes.push_back(make_name_prefix_blind_index(
            normalized.substr(0, end), key));
    }
    return indexes;
}

BlindIndex make_phone_blind_index(const std::string& phone,
                                  const std::string& key)
{
    return hmac_sha256(key, std::string(PHONE_INDEX_DOMAIN) +
                                normalize_phone(phone));
}

PatientService::PatientService(PatientRepository& repository,
                               std::string name_index_key,
                               std::string phone_index_key)
    : repository_(repository),
      name_index_key_(std::move(name_index_key)),
      phone_index_key_(std::move(phone_index_key))
{
    require_valid_key(name_index_key_);
    require_valid_key(phone_index_key_);
    if (name_index_key_ == phone_index_key_) {
        throw std::invalid_argument(
            "Patient name and phone blind-index keys must be different.");
    }
}

RegistrationResult PatientService::register_patient(
    const Registration& registration,
    const std::string& today) const
{
    RegistrationResult result;
    result.errors = validate_registration(registration, today);
    if (!result.errors.empty()) {
        return result;
    }

    const std::vector<BlindIndex> name_indexes =
        make_name_prefix_blind_indexes(registration.name, name_index_key_);
    const BlindIndex phone_index =
        make_phone_blind_index(registration.phone, phone_index_key_);
    result.patient = repository_.create_patient(
        registration, name_indexes, phone_index);
    result.saved = !result.patient.patient_id.empty();
    if (!result.saved) {
        result.errors.push_back(
            {"profile", "A patient profile is already linked to this account."});
    }
    return result;
}

RegistrationResult PatientService::update_patient(
    const std::string& patient_id,
    const Registration& registration,
    const std::string& today,
    const std::string& changed_by_user_id) const
{
    RegistrationResult result;
    result.errors = validate_registration(registration, today);
    if (!result.errors.empty()) {
        return result;
    }
    const auto name_indexes =
        make_name_prefix_blind_indexes(registration.name, name_index_key_);
    const auto phone_index =
        make_phone_blind_index(registration.phone, phone_index_key_);
    if (!repository_.update_patient(patient_id, registration, name_indexes,
                                    phone_index, changed_by_user_id)) {
        result.errors.push_back({"patientId", "Patient was not found."});
        return result;
    }
    result.saved = true;
    result.patient = {patient_id, registration};
    return result;
}

SearchResult PatientService::search_patients(const std::string& query) const
{
    const std::string trimmed = trim_ascii(query);
    SearchResult result;
    if (trimmed.empty()) {
        result.message = "Search query is required.";
        return result;
    }

    if (is_patient_id(trimmed)) {
        result.matches = repository_.find_by_id(trimmed);
    } else if (is_phone_query(trimmed)) {
        result.matches = repository_.find_by_phone_index(
            make_phone_blind_index(trimmed, phone_index_key_));
    } else {
        const std::string prefix = normalize_name_search(trimmed);
        if (prefix.empty()) {
            result.message = "Search query is required.";
            return result;
        }
        result.matches = repository_.find_by_name_prefix(
            make_name_prefix_blind_index(prefix, name_index_key_));
    }

    if (result.matches.empty()) {
        result.message = "No patient found";
    }
    return result;
}

} // namespace hms::patients
