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
#include <cctype>
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

} // namespace

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

AuthResult AuthService::authenticate_user(const std::string& username,
                                          const std::string& password)
{
    if (username.size() > 100 || password.size() > 1024) {
        return {false, "", "", "Invalid username or password."};
    }
    const std::optional<AuthUser> user = repository_.find_user(username);
    if (!user.has_value() || !user->active) {
        return {false, "", "", "Invalid username or password."};
    }
    if (user->account_locked) {
        return {false, "", "", "Account locked.", Role::Patient, true};
    }
    if (!verify_password(password, user->password_hash)) {
        if (repository_.record_failed_login(username)) {
            return {false, "", "", "Account locked.", Role::Patient, true};
        }
        return {false, "", "", "Invalid username or password."};
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

struct PostgresAuthRepository::Connection {
    PGconn* value = nullptr;
};

PostgresAuthRepository::PostgresAuthRepository(const char* connection_info)
    : connection_(new Connection)
{
    connection_->value = pg_connectdb(
        connection_info == nullptr ? "" : connection_info);
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

} // namespace hms
