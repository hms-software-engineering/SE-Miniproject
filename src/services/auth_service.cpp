#include "services/auth_service.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <argon2.h>

struct pg_conn;
struct pg_result;
using PGconn = pg_conn;
using PGresult = pg_result;
using ExecStatusType = int;

constexpr int CONNECTION_OK = 0;
constexpr int PGRES_COMMAND_OK = 1;
constexpr int PGRES_TUPLES_OK = 2;
constexpr int PGRES_FATAL_ERROR = 7;
constexpr int PG_DIAG_SQLSTATE = 'C';
#else
#include <argon2.h>
#include <libpq-fe.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hms {
namespace {

constexpr std::uint32_t PASSWORD_ITERATIONS = 600000;
constexpr std::size_t SALT_LENGTH = 16;
constexpr std::size_t PASSWORD_HASH_LENGTH = 32;
constexpr std::size_t TOKEN_LENGTH = 32;
constexpr char HEX_DIGITS[] = "0123456789abcdef";
constexpr std::uint32_t ARGON2_ITERATIONS = 3;
constexpr std::uint32_t ARGON2_MEMORY_KIB = 65536;
constexpr std::uint32_t ARGON2_PARALLELISM = 1;
constexpr std::size_t ARGON2_SALT_LENGTH = 16;
constexpr std::size_t ARGON2_HASH_LENGTH = 32;
constexpr char LEGACY_HASH_PREFIX[] = "pbkdf2-sha256$600000$";

using Digest = std::array<unsigned char, PASSWORD_HASH_LENGTH>;

#ifdef _WIN32
struct PostgresApi {
    HMODULE library = nullptr;
    PGconn* (__cdecl* connectdb)(const char*) = nullptr;
    int (__cdecl* status)(const PGconn*) = nullptr;
    char* (__cdecl* error_message)(const PGconn*) = nullptr;
    void (__cdecl* finish)(PGconn*) = nullptr;
    PGresult* (__cdecl* exec_params)(
        PGconn*, const char*, int, const unsigned int*,
        const char* const*, const int*, const int*, int) = nullptr;
    ExecStatusType (__cdecl* result_status)(const PGresult*) = nullptr;
    char* (__cdecl* result_error_message)(const PGresult*) = nullptr;
    char* (__cdecl* result_error_field)(const PGresult*, int) = nullptr;
    int (__cdecl* tuples)(const PGresult*) = nullptr;
    char* (__cdecl* value)(const PGresult*, int, int) = nullptr;
    void (__cdecl* clear)(PGresult*) = nullptr;

    PostgresApi()
    {
        library = LoadLibraryA("libpq.dll");
        if (library == nullptr) {
            throw std::runtime_error(
                "Could not load libpq.dll; add PostgreSQL's bin directory "
                "to PATH.");
        }
        connectdb = load<decltype(connectdb)>("PQconnectdb");
        status = load<decltype(status)>("PQstatus");
        error_message = load<decltype(error_message)>("PQerrorMessage");
        finish = load<decltype(finish)>("PQfinish");
        exec_params = load<decltype(exec_params)>("PQexecParams");
        result_status = load<decltype(result_status)>("PQresultStatus");
        result_error_message =
            load<decltype(result_error_message)>("PQresultErrorMessage");
        result_error_field = load<decltype(result_error_field)>(
            "PQresultErrorField");
        tuples = load<decltype(tuples)>("PQntuples");
        value = load<decltype(value)>("PQgetvalue");
        clear = load<decltype(clear)>("PQclear");
    }

    ~PostgresApi()
    {
        if (library != nullptr) {
            FreeLibrary(library);
        }
    }

private:
    template <typename Function>
    Function load(const char* name)
    {
        const auto address = GetProcAddress(library, name);
        if (address == nullptr) {
            throw std::runtime_error(
                std::string("PostgreSQL client is missing symbol ") + name);
        }
        static_assert(sizeof(Function) == sizeof(address),
                      "Windows API function pointers must have equal size.");
        Function function = nullptr;
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }
};

PostgresApi& postgres_api()
{
    static PostgresApi api;
    return api;
}

PGconn* pg_connectdb(const char* value)
{
    return postgres_api().connectdb(value);
}
int pg_status(const PGconn* value)
{
    return postgres_api().status(value);
}
char* pg_error_message(const PGconn* value)
{
    return postgres_api().error_message(value);
}
void pg_finish(PGconn* value)
{
    postgres_api().finish(value);
}
PGresult* pg_exec_params(PGconn* connection, const char* command, int count,
                         const unsigned int* types,
                         const char* const* values, const int* lengths,
                         const int* formats, int format)
{
    return postgres_api().exec_params(connection, command, count, types,
                                      values, lengths, formats, format);
}
ExecStatusType pg_result_status(const PGresult* value)
{
    return postgres_api().result_status(value);
}
char* pg_result_error_message(const PGresult* value)
{
    return postgres_api().result_error_message(value);
}
char* pg_result_error_field(const PGresult* value, int field)
{
    return postgres_api().result_error_field(value, field);
}
int pg_tuples(const PGresult* value)
{
    return postgres_api().tuples(value);
}
char* pg_value(const PGresult* value, int row, int column)
{
    return postgres_api().value(value, row, column);
}
void pg_clear(PGresult* value)
{
    postgres_api().clear(value);
}
#else
PGconn* pg_connectdb(const char* value) { return PQconnectdb(value); }
int pg_status(const PGconn* value) { return PQstatus(value); }
char* pg_error_message(const PGconn* value) { return PQerrorMessage(value); }
void pg_finish(PGconn* value) { PQfinish(value); }
PGresult* pg_exec_params(PGconn* connection, const char* command, int count,
                         const Oid* types, const char* const* values,
                         const int* lengths, const int* formats, int format)
{
    return PQexecParams(connection, command, count, types, values, lengths,
                        formats, format);
}
ExecStatusType pg_result_status(const PGresult* value)
{
    return PQresultStatus(value);
}
char* pg_result_error_message(const PGresult* value)
{
    return PQresultErrorMessage(value);
}
char* pg_result_error_field(const PGresult* value, int field)
{
    return PQresultErrorField(value, field);
}
int pg_tuples(const PGresult* value) { return PQntuples(value); }
char* pg_value(const PGresult* value, int row, int column)
{
    return PQgetvalue(value, row, column);
}
void pg_clear(PGresult* value) { PQclear(value); }
#endif

std::string encode_hex(const unsigned char* data, std::size_t length)
{
    std::string result;
    result.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        result.push_back(HEX_DIGITS[data[i] >> 4]);
        result.push_back(HEX_DIGITS[data[i] & 0x0f]);
    }
    return result;
}

bool decode_hex(const std::string& value, std::vector<unsigned char>& result)
{
    if (value.size() % 2 != 0) {
        return false;
    }
    result.clear();
    result.reserve(value.size() / 2);
    for (std::size_t i = 0; i < value.size(); i += 2) {
        const auto nibble = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            return -1;
        };
        const int high = nibble(value[i]);
        const int low = nibble(value[i + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        result.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return true;
}

bool secure_random(unsigned char* data, std::size_t length)
{
#ifdef _WIN32
    if (length > static_cast<std::size_t>(
                     std::numeric_limits<ULONG>::max())) {
        return false;
    }
    return BCryptGenRandom(nullptr, data, static_cast<ULONG>(length),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
#else
    return length <= static_cast<std::size_t>(
                         std::numeric_limits<int>::max()) &&
           RAND_bytes(data, static_cast<int>(length)) == 1;
#endif
}

bool derive_password_hash(const std::string& password,
                          const unsigned char* salt,
                          std::size_t salt_length,
                          Digest& digest)
{
#ifdef _WIN32
    if (password.size() > std::numeric_limits<ULONG>::max() ||
        salt_length > std::numeric_limits<ULONG>::max()) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
        BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status >= 0) {
        status = BCryptDeriveKeyPBKDF2(
            algorithm,
            reinterpret_cast<PUCHAR>(
                const_cast<char*>(password.data())),
            static_cast<ULONG>(password.size()),
            const_cast<PUCHAR>(salt),
            static_cast<ULONG>(salt_length),
            PASSWORD_ITERATIONS, digest.data(),
            static_cast<ULONG>(digest.size()), 0);
    }
    if (algorithm != nullptr) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    return status >= 0;
#else
    if (password.size() > static_cast<std::size_t>(
                              std::numeric_limits<int>::max()) ||
        salt_length > static_cast<std::size_t>(
                          std::numeric_limits<int>::max())) {
        return false;
    }
    return PKCS5_PBKDF2_HMAC(
               password.data(), static_cast<int>(password.size()), salt,
               static_cast<int>(salt_length), PASSWORD_ITERATIONS,
               EVP_sha256(), static_cast<int>(digest.size()),
               digest.data()) == 1;
#endif
}

bool verify_legacy_password(const std::string& password,
                            const std::string& encoded_hash)
{
    if (encoded_hash.compare(0, sizeof(LEGACY_HASH_PREFIX) - 1,
                             LEGACY_HASH_PREFIX) != 0) {
        return false;
    }
    const std::size_t separator =
        encoded_hash.find('$', sizeof(LEGACY_HASH_PREFIX) - 1);
    if (separator == std::string::npos) {
        return false;
    }
    std::vector<unsigned char> salt;
    std::vector<unsigned char> expected;
    if (!decode_hex(encoded_hash.substr(
                        sizeof(LEGACY_HASH_PREFIX) - 1,
                        separator - (sizeof(LEGACY_HASH_PREFIX) - 1)),
                    salt) ||
        salt.size() != SALT_LENGTH ||
        !decode_hex(encoded_hash.substr(separator + 1), expected) ||
        expected.size() != PASSWORD_HASH_LENGTH) {
        return false;
    }

    Digest actual{};
    if (!derive_password_hash(password, salt.data(), salt.size(), actual)) {
        throw std::runtime_error("Password verification failed.");
    }
    unsigned char difference = 0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        difference |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    }
    return difference == 0;
}

std::string hash_password(const std::string& password)
{
    const std::size_t encoded_length = argon2_encodedlen(
        ARGON2_ITERATIONS, ARGON2_MEMORY_KIB, ARGON2_PARALLELISM,
        ARGON2_SALT_LENGTH, ARGON2_HASH_LENGTH, Argon2_id);
    std::vector<char> encoded(encoded_length);
    std::array<unsigned char, ARGON2_SALT_LENGTH> salt{};
    if (!secure_random(salt.data(), salt.size())) {
        throw std::runtime_error("Password hashing failed.");
    }
    const int result = argon2id_hash_encoded(
        ARGON2_ITERATIONS, ARGON2_MEMORY_KIB, ARGON2_PARALLELISM,
        password.data(), password.size(), salt.data(), salt.size(),
        ARGON2_HASH_LENGTH, encoded.data(), encoded.size());
    if (result != ARGON2_OK) {
        throw std::runtime_error(
            std::string("Argon2id password hashing failed: ") +
            argon2_error_message(result));
    }
    return encoded.data();
}

bool is_legacy_password_hash(const std::string& encoded_hash)
{
    return encoded_hash.compare(0, sizeof(LEGACY_HASH_PREFIX) - 1,
                                LEGACY_HASH_PREFIX) == 0;
}

bool verify_password(const std::string& password,
                     const std::string& encoded_hash)
{
    if (encoded_hash.compare(0, 10, "$argon2id$") == 0) {
        return argon2id_verify(encoded_hash.c_str(), password.data(),
                                password.size()) == ARGON2_OK;
    }
    return verify_legacy_password(password, encoded_hash);
}

std::string hash_token(const std::string& token)
{
    std::array<unsigned char, 32> digest{};
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD digest_size = 0;
    DWORD bytes_read = 0;
    std::vector<unsigned char> object;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status >= 0) {
        status = BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
            &bytes_read, 0);
    }
    if (status >= 0) {
        status = BCryptGetProperty(
            algorithm, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&digest_size), sizeof(digest_size),
            &bytes_read, 0);
    }
    if (status >= 0 && digest_size != digest.size()) {
        status = static_cast<NTSTATUS>(-1);
    }
    if (status >= 0) {
        object.resize(object_size);
        status = BCryptCreateHash(algorithm, &hash, object.data(),
                                  object_size, nullptr, 0, 0);
    }
    if (status >= 0) {
        status = BCryptHashData(
            hash, reinterpret_cast<PUCHAR>(
                      const_cast<char*>(token.data())),
            static_cast<ULONG>(token.size()), 0);
    }
    if (status >= 0) {
        status = BCryptFinishHash(hash, digest.data(),
                                  static_cast<ULONG>(digest.size()), 0);
    }
    if (hash != nullptr) BCryptDestroyHash(hash);
    if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) {
        throw std::runtime_error("Session token hashing failed.");
    }
#else
    unsigned int digest_size = 0;
    if (EVP_Digest(token.data(), token.size(), digest.data(), &digest_size,
                   EVP_sha256(), nullptr) != 1 ||
        digest_size != digest.size()) {
        throw std::runtime_error("Session token hashing failed.");
    }
#endif
    return encode_hex(digest.data(), digest.size());
}

std::string make_session_token()
{
    std::array<unsigned char, TOKEN_LENGTH> token{};
    if (!secure_random(token.data(), token.size())) {
        throw std::runtime_error("Session token generation failed.");
    }
    return encode_hex(token.data(), token.size());
}

bool valid_username(const std::string& username)
{
    if (username.empty() || username.size() > 100) {
        return false;
    }
    return std::none_of(username.begin(), username.end(),
                        [](unsigned char ch) {
                            return std::iscntrl(ch) != 0;
                        });
}

bool valid_password(const std::string& password)
{
    if (password.size() < 8 || password.size() > 1024) {
        return false;
    }
    bool upper = false;
    bool lower = false;
    bool digit = false;
    bool special = false;
    for (char raw_character : password) {
        const auto ch = static_cast<unsigned char>(raw_character);
        upper |= std::isupper(ch) != 0;
        lower |= std::islower(ch) != 0;
        digit |= std::isdigit(ch) != 0;
        special |= std::ispunct(ch) != 0 || std::isspace(ch) != 0;
    }
    return upper && lower && digit && special;
}

class Result {
public:
    explicit Result(PGresult* result) : result_(result) {}
    ~Result() { pg_clear(result_); }
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    PGresult* get() const { return result_; }

private:
    PGresult* result_;
};

void require_status(PGconn* connection, PGresult* result,
                    ExecStatusType expected, const char* operation)
{
    if (result == nullptr || pg_result_status(result) != expected) {
        const std::string details =
            result == nullptr ? pg_error_message(connection)
                              : pg_result_error_message(result);
        if (result != nullptr) {
            pg_clear(result);
        }
        throw std::runtime_error(std::string(operation) + ": " + details);
    }
}

void execute_command(PGconn* connection, const char* sql,
                     const char* operation)
{
    PGresult* raw = pg_exec_params(
        connection, sql, 0, nullptr, nullptr, nullptr, nullptr, 0);
    require_status(connection, raw, PGRES_COMMAND_OK, operation);
    Result result(raw);
}

void rollback_transaction(PGconn* connection) noexcept
{
    PGresult* result = pg_exec_params(
        connection, "ROLLBACK", 0, nullptr, nullptr, nullptr, nullptr, 0);
    if (result != nullptr) {
        pg_clear(result);
    }
}

void insert_audit_event(PGconn* connection, const AuditEvent& event)
{
    const std::string user_id = event.user_id()
        ? std::to_string(*event.user_id()) : std::string();
    const std::string record_id = event.record_id()
        ? event.record_id()->value() : std::string();
    const char* values[] = {
        user_id.c_str(), audit_role_name(event.role()),
        audit_action_name(event.action()), record_id.c_str(),
        event.source_ip().c_str()
    };
    PGresult* raw = pg_exec_params(
        connection,
        "INSERT INTO audit_log(user_id,role,action,record_id,source_ip) "
        "VALUES(NULLIF($1,'')::bigint,$2,$3,NULLIF($4,''),"
        "NULLIF($5,'')::inet)",
        5, nullptr, values, nullptr, nullptr, 0);
    require_status(connection, raw, PGRES_COMMAND_OK,
                   "Could not write audit event");
    Result result(raw);
}

} // namespace

std::optional<AuditRecordId> AuditRecordId::parse(const std::string& value)
{
    const auto digits = [](const std::string& text, std::size_t start) {
        return std::all_of(text.begin() + static_cast<std::ptrdiff_t>(start),
                           text.end(), [](unsigned char ch) {
                               return ch >= '0' && ch <= '9';
                           });
    };
    bool numeric = !value.empty() && value.size() <= 20 && digits(value, 0);
    if (numeric) {
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        numeric = result.ec == std::errc{} &&
                  result.ptr == value.data() + value.size() && parsed > 0 &&
                  parsed <= static_cast<std::uint64_t>(
                      std::numeric_limits<std::int64_t>::max());
    }
    const bool patient = value.size() == 12 && value.compare(0, 4, "PID-") == 0 &&
                         digits(value, 4);
    if (!numeric && !patient) return std::nullopt;
    return AuditRecordId(value);
}

AuditRole audit_role(Role role) noexcept
{
    switch (role) {
    case Role::Patient: return AuditRole::Patient;
    case Role::Receptionist: return AuditRole::Receptionist;
    case Role::Doctor: return AuditRole::Doctor;
    case Role::BillingOfficer: return AuditRole::BillingOfficer;
    case Role::Administrator: return AuditRole::Administrator;
    }
    return AuditRole::Unknown;
}

const char* audit_role_name(AuditRole role) noexcept
{
    switch (role) {
    case AuditRole::Unknown: return "Unknown";
    case AuditRole::Patient: return "Patient";
    case AuditRole::Receptionist: return "Receptionist";
    case AuditRole::Doctor: return "Doctor";
    case AuditRole::BillingOfficer: return "Billing Officer";
    case AuditRole::Administrator: return "Administrator";
    }
    return "Unknown";
}

const char* audit_action_name(AuditAction action) noexcept
{
    switch (action) {
    case AuditAction::CreateUser: return "CREATE_USER";
    case AuditAction::LoginFailed: return "LOGIN_FAILED";
    case AuditAction::Login: return "LOGIN";
    case AuditAction::Logout: return "LOGOUT";
    case AuditAction::ViewAccount: return "VIEW_ACCOUNT";
    case AuditAction::ViewAuditLog: return "VIEW_AUDIT_LOG";
    case AuditAction::GenerateDailyReport: return "GENERATE_DAILY_REPORT";
    case AuditAction::ViewPatientHistory: return "VIEW_PATIENT_HISTORY";
    case AuditAction::ViewPatient: return "VIEW_PATIENT";
    case AuditAction::CreatePatientProfile: return "CREATE_PATIENT_PROFILE";
    case AuditAction::ViewOwnSchedule: return "VIEW_OWN_SCHEDULE";
    case AuditAction::ViewFreeSlots: return "VIEW_FREE_SLOTS";
    case AuditAction::CreateAppointment: return "CREATE_APPOINTMENT";
    case AuditAction::CancelAppointment: return "CANCEL_APPOINTMENT";
    case AuditAction::RescheduleAppointment: return "RESCHEDULE_APPOINTMENT";
    case AuditAction::CreateVisit: return "CREATE_VISIT";
    case AuditAction::CreatePrescription: return "CREATE_PRESCRIPTION";
    case AuditAction::CreatePatient: return "CREATE_PATIENT";
    case AuditAction::UpdatePatient: return "UPDATE_PATIENT";
    case AuditAction::DeactivateUser: return "DEACTIVATE_USER";
    case AuditAction::CreateBill: return "CREATE_BILL";
    case AuditAction::RecordPayment: return "RECORD_PAYMENT";
    case AuditAction::UpdatePatientAllergies: return "UPDATE_PATIENT_ALLERGIES";
    case AuditAction::OverrideAllergyWarning: return "OVERRIDE_ALLERGY_WARNING";
    case AuditAction::ViewVisitHistory: return "VIEW_VISIT_HISTORY";
    case AuditAction::ViewOwnHistory: return "VIEW_OWN_HISTORY";
    case AuditAction::ViewOwnBills: return "VIEW_OWN_BILLS";
    }
    return "";
}

std::optional<AuditRole> parse_audit_role(const std::string& value)
{
    for (AuditRole role : {AuditRole::Unknown, AuditRole::Patient,
                           AuditRole::Receptionist, AuditRole::Doctor,
                           AuditRole::BillingOfficer,
                           AuditRole::Administrator}) {
        if (value == audit_role_name(role)) return role;
    }
    return std::nullopt;
}

std::optional<AuditAction> parse_audit_action(const std::string& value)
{
    for (AuditAction action : {
             AuditAction::CreateUser, AuditAction::LoginFailed,
             AuditAction::Login, AuditAction::Logout, AuditAction::ViewAccount,
             AuditAction::ViewAuditLog,
             AuditAction::GenerateDailyReport, AuditAction::ViewPatientHistory,
             AuditAction::ViewPatient, AuditAction::CreatePatientProfile,
             AuditAction::ViewOwnSchedule, AuditAction::ViewFreeSlots,
             AuditAction::CreateAppointment, AuditAction::CancelAppointment,
             AuditAction::RescheduleAppointment, AuditAction::CreateVisit,
             AuditAction::CreatePrescription, AuditAction::CreatePatient,
             AuditAction::UpdatePatient, AuditAction::DeactivateUser,
             AuditAction::CreateBill,
             AuditAction::RecordPayment,
             AuditAction::UpdatePatientAllergies,
             AuditAction::OverrideAllergyWarning,
             AuditAction::ViewVisitHistory,
             AuditAction::ViewOwnHistory,
             AuditAction::ViewOwnBills}) {
        if (value == audit_action_name(action)) return action;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> parse_audit_user_id(const std::string& value)
{
    if (value.empty() || value.size() > 20 ||
        !std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        })) return std::nullopt;
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        parsed == 0 || parsed > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()))
        return std::nullopt;
    return parsed;
}

AuditEvent make_audit_event(const std::string& user_id,
                            AuditRole role,
                            AuditAction action,
                            const std::string& record_id,
                            const std::string& source_ip)
{
    return AuditEvent(user_id, audit_role_name(role), audit_action_name(action),
                      record_id, source_ip);
}

AuditEvent::AuditEvent(const std::string& raw_user_id,
                       const std::string& raw_role,
                       const std::string& raw_action,
                       const std::string& raw_record_id,
                       const std::string& raw_source_ip)
{
    const auto role_value = parse_audit_role(raw_role);
    const auto action_value = parse_audit_action(raw_action);
    if (!role_value || !action_value)
        throw std::invalid_argument("Unsupported audit metadata.");
    const auto parsed_user = raw_user_id.empty()
        ? std::optional<std::uint64_t>{}
        : parse_audit_user_id(raw_user_id);
    const auto parsed_record = raw_record_id.empty()
        ? std::optional<AuditRecordId>{}
        : AuditRecordId::parse(raw_record_id);
    if ((!raw_user_id.empty() && !parsed_user) ||
        (!raw_record_id.empty() && !parsed_record) ||
        raw_source_ip.empty() || raw_source_ip.size() > 45 ||
        !std::all_of(raw_source_ip.begin(), raw_source_ip.end(),
            [](unsigned char ch) {
                return (ch >= '0' && ch <= '9') ||
                       (ch >= 'a' && ch <= 'f') ||
                       (ch >= 'A' && ch <= 'F') || ch == '.' || ch == ':';
            })) {
        throw std::invalid_argument("Invalid audit metadata.");
    }
    user_id_ = parsed_user;
    role_ = *role_value;
    action_ = *action_value;
    record_id_ = parsed_record;
    source_ip_ = raw_source_ip;
}

AuthService::AuthService(AuthRepository& repository)
    : repository_(repository)
{
}

AuthResult AuthService::register_patient(const std::string& username,
                                         const std::string& password)
{
    return create_user(username, password, Role::Patient);
}

AuthResult AuthService::create_user(const std::string& username,
                                    const std::string& password,
                                    Role role)
{
    if (!valid_username(username)) {
        return {false, "", "", "Username must contain 1 to 100 valid characters."};
    }
    if (!valid_password(password)) {
        return {false, "", "",
                "Password must be at least 8 characters and include uppercase, "
                "lowercase, digit, and special character."};
    }
    if (role_name(role) == "Unknown") {
        return {false, "", "", "A valid role is required."};
    }

    const std::string encoded_hash = hash_password(password);
    std::string user_id;
    if (repository_.create_user(username, encoded_hash, role, user_id) ==
        CreateUserStatus::AlreadyExists) {
        return {false, "", "", "Username already exists."};
    }
    return {true, user_id, "", "User created.", role};
}

DeactivateUserStatus AuthService::deactivate_user(
    const SessionPrincipal& actor,
    const std::string& user_id,
    const std::string& source_ip)
{
    if (!is_permitted(actor.role, Permission::ManageUsers)) {
        return DeactivateUserStatus::ActorNotAuthorized;
    }
    const AuditEvent audit_event = make_audit_event(
        actor.user_id, audit_role(actor.role), AuditAction::DeactivateUser,
        user_id, source_ip);
    return repository_.deactivate_user(actor.user_id, user_id, audit_event);
}

AuthResult AuthService::authenticate_user(const std::string& username,
                                          const std::string& password)
{
    if (username.size() > 100 || password.size() > 1024) {
        return {false, "", "", "Invalid username or password."};
    }
    const std::optional<AuthUser> user = repository_.find_user(username);
    if (!user.has_value()) {
        return {false, "", "", "Invalid username or password."};
    }
    if (!user->active) {
        return {false, user->user_id, "",
                "Invalid username or password.", user->role};
    }
    if (user->account_locked) {
        return {false, user->user_id, "", "Account locked.", user->role, true};
    }
    if (!verify_password(password, user->password_hash)) {
        if (repository_.record_failed_login(username)) {
            return {false, user->user_id, "", "Account locked.", user->role,
                    true};
        }
        return {false, user->user_id, "",
                "Invalid username or password.", user->role};
    }

    if (is_legacy_password_hash(user->password_hash)) {
        repository_.update_password_hash(username, hash_password(password));
    }
    repository_.reset_failed_logins(username);
    const std::string token = make_session_token();
    repository_.create_session(hash_token(token), user->user_id);
    return {true, user->user_id, token, "Login successful.", user->role};
}

std::optional<SessionPrincipal> AuthService::authenticate_token(
    const std::string& token) const
{
    if (token.size() != TOKEN_LENGTH * 2) {
        return std::nullopt;
    }
    std::vector<unsigned char> decoded;
    if (!decode_hex(token, decoded) || decoded.size() != TOKEN_LENGTH) {
        return std::nullopt;
    }
    return repository_.find_session(hash_token(token));
}

void AuthService::logout(const std::string& token)
{
    if (token.size() != TOKEN_LENGTH * 2) {
        return;
    }
    std::vector<unsigned char> decoded;
    if (decode_hex(token, decoded) && decoded.size() == TOKEN_LENGTH) {
        repository_.delete_session(hash_token(token));
    }
}

void AuthService::record_audit_event(const AuditEvent& event)
{
    repository_.record_audit_event(event);
}

AuditReadResult AuthService::read_audit_events(
    const std::string& token,
    std::optional<AuditAction> action,
    std::optional<AuditRecordId> record_id) const
{
    const auto principal = authenticate_token(token);
    if (!principal)
        return {AuditReadStatus::AuthenticationRequired, {}};
    std::string authorization_error;
    if (!authorize(principal->role, Permission::ViewAuditLog,
                   authorization_error))
        return {AuditReadStatus::AccessDenied, {}};
    return {AuditReadStatus::Success,
            repository_.read_audit_events(std::move(action),
                                          std::move(record_id))};
}

struct PostgresAuthRepository::Connection {
    PGconn* value = nullptr;
    std::string connection_info;
};

PostgresAuthRepository::PostgresAuthRepository(const char* connection_info)
    : connection_(new Connection)
{
    connection_->connection_info =
        connection_info == nullptr ? "" : connection_info;
    connection_->value = pg_connectdb(
        connection_->connection_info.c_str());
    if (connection_->value == nullptr ||
        pg_status(connection_->value) != CONNECTION_OK) {
        const std::string details = connection_->value == nullptr
            ? "Could not allocate PostgreSQL connection."
            : pg_error_message(connection_->value);
        if (connection_->value != nullptr) {
            pg_finish(connection_->value);
        }
        delete connection_;
        connection_ = nullptr;
        throw std::runtime_error("PostgreSQL connection failed: " + details);
    }
}

PostgresAuthRepository::~PostgresAuthRepository()
{
    if (connection_ != nullptr) {
        pg_finish(connection_->value);
        delete connection_;
    }
}

bool PostgresAuthRepository::health_check() const noexcept
{
    if (connection_ == nullptr) {
        return false;
    }

    // A separate, short-lived connection allows a later probe to recover
    // after PostgreSQL returns without disturbing authentication queries.
    PGconn* connection = nullptr;
    try {
        std::string probe_conninfo = connection_->connection_info;
        const bool is_uri = probe_conninfo.rfind("postgresql://", 0) == 0 ||
                            probe_conninfo.rfind("postgres://", 0) == 0;
        if (is_uri) {
            const std::string separator =
                probe_conninfo.find('?') == std::string::npos ? "?" : "&";
            probe_conninfo += separator +
                "connect_timeout=2&options=-c%20statement_timeout%3D1000ms";
        } else {
            probe_conninfo +=
                " connect_timeout=2 options='-c statement_timeout=1000ms'";
        }

        connection = pg_connectdb(probe_conninfo.c_str());
        if (connection == nullptr || pg_status(connection) != CONNECTION_OK) {
            if (connection != nullptr) {
                pg_finish(connection);
            }
            return false;
        }

        PGresult* raw = pg_exec_params(
            connection, "SELECT 1", 0, nullptr, nullptr, nullptr, nullptr, 0);
        require_status(connection, raw, PGRES_TUPLES_OK,
                       "PostgreSQL health check failed");
        Result result(raw);
        const bool healthy = pg_tuples(result.get()) == 1 &&
                             std::string(pg_value(result.get(), 0, 0)) == "1";
        pg_finish(connection);
        return healthy;
    } catch (...) {
        if (connection != nullptr) {
            pg_finish(connection);
        }
        return false;
    }
}

CreateUserStatus PostgresAuthRepository::create_user(
    const std::string& username,
    const std::string& password_hash,
    Role role,
    std::string& user_id)
{
    const std::string role_value(role_name(role));
    const char* values[] = {username.c_str(), password_hash.c_str(),
                            role_value.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "WITH new_user AS ("
        "INSERT INTO users (username, password_hash, role) "
        "VALUES ($1, $2, $3) RETURNING user_id, role"
        "), new_doctor AS ("
        "INSERT INTO doctors (user_id, specialty, fee, working_hours) "
        "SELECT user_id, 'General Medicine', 0, "
        "'{\"monday\":\"09:00-17:00\",\"tuesday\":\"09:00-17:00\","
        "\"wednesday\":\"09:00-17:00\",\"thursday\":\"09:00-17:00\","
        "\"friday\":\"09:00-17:00\"}'::jsonb "
        "FROM new_user WHERE role = 'Doctor' RETURNING doctor_id"
        ") SELECT user_id::text FROM new_user",
        3, nullptr, values, nullptr, nullptr, 0);
    if (raw != nullptr &&
        pg_result_status(raw) == PGRES_FATAL_ERROR &&
        std::string(pg_result_error_field(raw, PG_DIAG_SQLSTATE) == nullptr
                        ? ""
                        : pg_result_error_field(raw, PG_DIAG_SQLSTATE)) ==
            "23505") {
        pg_clear(raw);
        return CreateUserStatus::AlreadyExists;
    }
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not create user");
    Result result(raw);
    user_id = pg_value(result.get(), 0, 0);
    return CreateUserStatus::Created;
}

DeactivateUserStatus PostgresAuthRepository::deactivate_user(
    const std::string& actor_user_id,
    const std::string& user_id,
    const AuditEvent& audit_event)
{
    PGconn* connection = connection_->value;
    execute_command(connection, "BEGIN", "Could not begin deactivation");
    try {
        // Serialize account writes while checking the last active
        // Administrator, including simultaneous deactivation requests.
        execute_command(connection,
                        "LOCK TABLE users IN SHARE ROW EXCLUSIVE MODE",
                        "Could not lock user accounts");

        const char* actor_values[] = {actor_user_id.c_str()};
        PGresult* actor_raw = pg_exec_params(
            connection,
            "SELECT role, active FROM users WHERE user_id = $1::bigint",
            1, nullptr, actor_values, nullptr, nullptr, 0);
        require_status(connection, actor_raw, PGRES_TUPLES_OK,
                       "Could not verify administrator account");
        Result actor_result(actor_raw);
        if (pg_tuples(actor_result.get()) != 1 ||
            std::string(pg_value(actor_result.get(), 0, 0)) !=
                "Administrator" ||
            std::string(pg_value(actor_result.get(), 0, 1)) != "t") {
            rollback_transaction(connection);
            return DeactivateUserStatus::ActorNotAuthorized;
        }

        if (actor_user_id == user_id) {
            rollback_transaction(connection);
            return DeactivateUserStatus::SelfDeactivation;
        }

        const char* target_values[] = {user_id.c_str()};
        PGresult* target_raw = pg_exec_params(
            connection,
            "SELECT role, active FROM users WHERE user_id = $1::bigint",
            1, nullptr, target_values, nullptr, nullptr, 0);
        require_status(connection, target_raw, PGRES_TUPLES_OK,
                       "Could not find user to deactivate");
        Result target_result(target_raw);
        if (pg_tuples(target_result.get()) != 1) {
            rollback_transaction(connection);
            return DeactivateUserStatus::NotFound;
        }

        const bool target_is_active_admin =
            std::string(pg_value(target_result.get(), 0, 0)) ==
                "Administrator" &&
            std::string(pg_value(target_result.get(), 0, 1)) == "t";
        if (target_is_active_admin) {
            PGresult* count_raw = pg_exec_params(
                connection,
                "SELECT COUNT(*) FROM users "
                "WHERE role = 'Administrator' AND active = TRUE",
                0, nullptr, nullptr, nullptr, nullptr, 0);
            require_status(connection, count_raw, PGRES_TUPLES_OK,
                           "Could not count active administrators");
            Result count_result(count_raw);
            if (std::stoll(pg_value(count_result.get(), 0, 0)) <= 1) {
                rollback_transaction(connection);
                return DeactivateUserStatus::LastAdministrator;
            }
        }

        PGresult* update_raw = pg_exec_params(
            connection,
            "UPDATE users SET active = FALSE, "
            "updated_at = CURRENT_TIMESTAMP WHERE user_id = $1::bigint "
            "RETURNING user_id",
            1, nullptr, target_values, nullptr, nullptr, 0);
        require_status(connection, update_raw, PGRES_TUPLES_OK,
                       "Could not deactivate user");
        Result update_result(update_raw);
        if (pg_tuples(update_result.get()) != 1) {
            rollback_transaction(connection);
            return DeactivateUserStatus::NotFound;
        }

        insert_audit_event(connection, audit_event);
        execute_command(connection, "COMMIT", "Could not commit deactivation");
        return DeactivateUserStatus::Deactivated;
    } catch (...) {
        rollback_transaction(connection);
        throw;
    }
}

std::optional<AuthUser> PostgresAuthRepository::find_user(
    const std::string& username) const
{
    const char* values[] = {username.c_str()};
    PGresult* expired_raw = pg_exec_params(
        connection_->value,
        "UPDATE users SET failed_attempts = 0, locked_until = NULL "
        "WHERE username = $1 AND locked_until IS NOT NULL "
        "AND locked_until <= CURRENT_TIMESTAMP",
        1, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, expired_raw, PGRES_COMMAND_OK,
                   "Could not clear expired login lock");
    pg_clear(expired_raw);

    PGresult* raw = pg_exec_params(
        connection_->value,
        "SELECT user_id::text, username, password_hash, role, active, "
        "(locked_until > CURRENT_TIMESTAMP) "
        "FROM users WHERE username = $1",
        1, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not find user");
    Result result(raw);
    if (pg_tuples(result.get()) == 0) {
        return std::nullopt;
    }

    Role role;
    if (!parse_role(pg_value(result.get(), 0, 3), role)) {
        throw std::runtime_error("Database contains an unknown user role.");
    }
    return AuthUser{
        pg_value(result.get(), 0, 0),
        pg_value(result.get(), 0, 1),
        pg_value(result.get(), 0, 2),
        role,
        std::string(pg_value(result.get(), 0, 4)) == "t",
        std::string(pg_value(result.get(), 0, 5)) == "t"
    };
}

bool PostgresAuthRepository::record_failed_login(
    const std::string& username)
{
    const char* values[] = {username.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "UPDATE users SET "
        "failed_attempts = LEAST(failed_attempts + 1, 5), "
        "locked_until = CASE WHEN failed_attempts + 1 >= 5 "
        "THEN CURRENT_TIMESTAMP + INTERVAL '15 minutes' "
        "ELSE locked_until END "
        "WHERE username = $1 AND active = TRUE "
        "RETURNING locked_until > CURRENT_TIMESTAMP",
        1, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not record failed login");
    Result result(raw);
    return pg_tuples(result.get()) > 0 &&
           std::string(pg_value(result.get(), 0, 0)) == "t";
}

void PostgresAuthRepository::reset_failed_logins(
    const std::string& username)
{
    const char* values[] = {username.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "UPDATE users SET failed_attempts = 0, locked_until = NULL "
        "WHERE username = $1",
        1, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_COMMAND_OK,
                   "Could not reset failed login count");
    pg_clear(raw);
}

void PostgresAuthRepository::update_password_hash(
    const std::string& username, const std::string& password_hash)
{
    const char* values[] = {username.c_str(), password_hash.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "UPDATE users SET password_hash = $2, updated_at = CURRENT_TIMESTAMP "
        "WHERE username = $1 AND active = TRUE RETURNING user_id",
        2, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not upgrade password hash");
    Result result(raw);
    if (pg_tuples(result.get()) != 1) {
        throw std::runtime_error(
            "Could not upgrade password hash: user is no longer active.");
    }
}

void PostgresAuthRepository::create_session(const std::string& token_hash,
                                            const std::string& user_id)
{
    const char* values[] = {token_hash.c_str(), user_id.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "INSERT INTO auth_sessions (token_hash, user_id, expires_at) "
        "VALUES ($1, $2::bigint, CURRENT_TIMESTAMP + INTERVAL '10 minutes')",
        2, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_COMMAND_OK,
                   "Could not create session");
    pg_clear(raw);
}

std::optional<SessionPrincipal> PostgresAuthRepository::find_session(
    const std::string& token_hash) const
{
    const char* values[] = {token_hash.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "UPDATE auth_sessions s "
        "SET expires_at = CURRENT_TIMESTAMP + INTERVAL '10 minutes' "
        "FROM users u "
        "WHERE s.token_hash = $1 AND s.expires_at > CURRENT_TIMESTAMP "
        "AND u.user_id = s.user_id AND u.active = TRUE "
        "RETURNING s.user_id::text, u.role",
        1, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not verify session");
    Result result(raw);
    if (pg_tuples(result.get()) == 0) {
        return std::nullopt;
    }

    Role role;
    if (!parse_role(pg_value(result.get(), 0, 1), role)) {
        throw std::runtime_error("Database contains an unknown session role.");
    }
    return SessionPrincipal{pg_value(result.get(), 0, 0), role};
}

void PostgresAuthRepository::delete_session(const std::string& token_hash)
{
    const char* values[] = {token_hash.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "DELETE FROM auth_sessions WHERE token_hash = $1",
        1, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_COMMAND_OK,
                   "Could not revoke session");
    pg_clear(raw);
}

void PostgresAuthRepository::record_audit_event(const AuditEvent& event)
{
    insert_audit_event(connection_->value, event);
}

std::vector<AuditRecord> PostgresAuthRepository::read_audit_events(
    std::optional<AuditAction> action,
    std::optional<AuditRecordId> record_id) const
{
    const char* action_value = action ? audit_action_name(*action) : nullptr;
    const char* record_value = record_id ? record_id->value().c_str() : nullptr;
    const char* values[] = {action_value, record_value};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "SELECT COALESCE(user_id::text,''), role, action, "
        "COALESCE(record_id,''), "
        "to_char(\"timestamp\" AT TIME ZONE 'UTC', "
        "'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
        "COALESCE(host(source_ip),'') FROM audit_log "
        "WHERE ($1::varchar IS NULL OR action = $1) "
        "AND ($2::varchar IS NULL OR record_id = $2) "
        "ORDER BY \"timestamp\" DESC, audit_id DESC",
        2, nullptr, values, nullptr, nullptr, 0);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not read audit events");
    Result result(raw);
    std::vector<AuditRecord> records;
    records.reserve(static_cast<std::size_t>(pg_tuples(result.get())));
    for (int row = 0; row < pg_tuples(result.get()); ++row) {
        const std::string user_text = pg_value(result.get(), row, 0);
        const auto role = parse_audit_role(pg_value(result.get(), row, 1));
        const auto parsed_action = parse_audit_action(pg_value(result.get(), row, 2));
        const std::string record_text = pg_value(result.get(), row, 3);
        const std::string source_ip = pg_value(result.get(), row, 5);
        const auto user_id = user_text.empty()
            ? std::optional<std::uint64_t>{}
            : parse_audit_user_id(user_text);
        const auto parsed_record = record_text.empty()
            ? std::optional<AuditRecordId>{}
            : AuditRecordId::parse(record_text);
        if ((!user_text.empty() && !user_id) || !role || !parsed_action ||
            (!record_text.empty() && !parsed_record)) {
            throw std::runtime_error("Invalid audit record in database.");
        }
        records.push_back({user_id, *role, *parsed_action, parsed_record,
                           pg_value(result.get(), row, 4), source_ip});
    }
    return records;
}

} // namespace hms
