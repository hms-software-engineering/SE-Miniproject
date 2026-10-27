#include "patients/patient_service.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

struct pg_conn;
struct pg_result;
using PGconn = pg_conn;
using PGresult = pg_result;
#else
#include <libpq-fe.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace hms::patients {
namespace {

constexpr int CONNECTION_OK = 0;
constexpr int PGRES_COMMAND_OK = 1;
constexpr int PGRES_TUPLES_OK = 2;
constexpr std::size_t AES_KEY_SIZE = 32;
constexpr std::size_t GCM_NONCE_SIZE = 12;
constexpr std::size_t GCM_TAG_SIZE = 16;
constexpr char HEX_DIGITS[] = "0123456789abcdef";

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
    int (__cdecl* result_status)(const PGresult*) = nullptr;
    char* (__cdecl* result_error_message)(const PGresult*) = nullptr;
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
        try {
            connectdb = load<decltype(connectdb)>("PQconnectdb");
            status = load<decltype(status)>("PQstatus");
            error_message = load<decltype(error_message)>("PQerrorMessage");
            finish = load<decltype(finish)>("PQfinish");
            exec_params = load<decltype(exec_params)>("PQexecParams");
            result_status = load<decltype(result_status)>("PQresultStatus");
            result_error_message =
                load<decltype(result_error_message)>("PQresultErrorMessage");
            tuples = load<decltype(tuples)>("PQntuples");
            value = load<decltype(value)>("PQgetvalue");
            clear = load<decltype(clear)>("PQclear");
        } catch (...) {
            FreeLibrary(library);
            library = nullptr;
            throw;
        }
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
                         const char* const* values)
{
    return postgres_api().exec_params(connection, command, count, nullptr,
                                      values, nullptr, nullptr, 0);
}
int pg_result_status(const PGresult* value)
{
    return postgres_api().result_status(value);
}
char* pg_result_error_message(const PGresult* value)
{
    return postgres_api().result_error_message(value);
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
                         const char* const* values)
{
    return PQexecParams(connection, command, count, nullptr, values, nullptr,
                        nullptr, 0);
}
int pg_result_status(const PGresult* value) { return PQresultStatus(value); }
char* pg_result_error_message(const PGresult* value)
{
    return PQresultErrorMessage(value);
}
int pg_tuples(const PGresult* value) { return PQntuples(value); }
char* pg_value(const PGresult* value, int row, int column)
{
    return PQgetvalue(value, row, column);
}
void pg_clear(PGresult* value) { PQclear(value); }
#endif

class Result {
public:
    explicit Result(PGresult* value) : value_(value) {}
    ~Result()
    {
        if (value_ != nullptr) {
            pg_clear(value_);
        }
    }
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    PGresult* get() const { return value_; }

private:
    PGresult* value_;
};

void require_status(PGconn* connection,
                    PGresult* result,
                    int expected,
                    const char* operation)
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

std::string encode_hex(const unsigned char* data, std::size_t length)
{
    std::string result;
    result.reserve(length * 2);
    for (std::size_t index = 0; index < length; ++index) {
        result.push_back(HEX_DIGITS[data[index] >> 4]);
        result.push_back(HEX_DIGITS[data[index] & 0x0f]);
    }
    return result;
}

bool decode_hex(const std::string& value, std::vector<unsigned char>& output)
{
    if (value.size() % 2 != 0) {
        return false;
    }
    const auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'f') return character - 'a' + 10;
        if (character >= 'A' && character <= 'F') return character - 'A' + 10;
        return -1;
    };
    output.clear();
    output.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2) {
        const int high = nibble(value[index]);
        const int low = nibble(value[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        output.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return true;
}

void secure_random(unsigned char* output, std::size_t length)
{
#ifdef _WIN32
    if (length > std::numeric_limits<ULONG>::max() ||
        BCryptGenRandom(nullptr, output, static_cast<ULONG>(length),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        throw std::runtime_error("Could not generate patient encryption nonce.");
    }
#else
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        RAND_bytes(output, static_cast<int>(length)) != 1) {
        throw std::runtime_error("Could not generate patient encryption nonce.");
    }
#endif
}

std::vector<unsigned char> encrypt_value(const std::string& plaintext,
                                         const std::string& key,
                                         const std::string& context)
{
    if (plaintext.size() > std::numeric_limits<int>::max() ||
        context.size() > std::numeric_limits<int>::max()) {
        throw std::length_error("Patient data is too large to encrypt.");
    }

    std::array<unsigned char, GCM_NONCE_SIZE> nonce{};
    std::array<unsigned char, GCM_TAG_SIZE> tag{};
    secure_random(nonce.data(), nonce.size());
    std::vector<unsigned char> ciphertext(plaintext.size());

#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE aes_key = nullptr;
    std::vector<unsigned char> key_object;
    DWORD object_size = 0;
    DWORD result_size = 0;
    DWORD bytes_read = 0;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status >= 0) {
        status = BCryptSetProperty(
            algorithm, BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(
                BCRYPT_CHAIN_MODE_GCM)),
            static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)), 0);
    }
    if (status >= 0) {
        status = BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
            &bytes_read, 0);
    }
    if (status >= 0) {
        key_object.resize(object_size);
        status = BCryptGenerateSymmetricKey(
            algorithm, &aes_key, key_object.data(), object_size,
            reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
            static_cast<ULONG>(key.size()), 0);
    }
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = nonce.data();
    auth_info.cbNonce = static_cast<ULONG>(nonce.size());
    auth_info.pbTag = tag.data();
    auth_info.cbTag = static_cast<ULONG>(tag.size());
    auth_info.pbAuthData = reinterpret_cast<PUCHAR>(
        const_cast<char*>(context.data()));
    auth_info.cbAuthData = static_cast<ULONG>(context.size());
    if (status >= 0) {
        status = BCryptEncrypt(
            aes_key,
            reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
            static_cast<ULONG>(plaintext.size()), &auth_info, nullptr, 0,
            ciphertext.data(), static_cast<ULONG>(ciphertext.size()),
            &result_size, 0);
    }
    if (aes_key != nullptr) BCryptDestroyKey(aes_key);
    if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0 || result_size != ciphertext.size()) {
        throw std::runtime_error("Could not encrypt patient data.");
    }
#else
    using CipherContext = std::unique_ptr<EVP_CIPHER_CTX,
                                          decltype(&EVP_CIPHER_CTX_free)>;
    CipherContext cipher(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int produced = 0;
    int final_bytes = 0;
    if (!cipher ||
        EVP_EncryptInit_ex(cipher.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                           nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(cipher.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1 ||
        EVP_EncryptInit_ex(
            cipher.get(), nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.data()),
            nonce.data()) != 1 ||
        EVP_EncryptUpdate(
            cipher.get(), nullptr, &produced,
            reinterpret_cast<const unsigned char*>(context.data()),
            static_cast<int>(context.size())) != 1 ||
        EVP_EncryptUpdate(
            cipher.get(), ciphertext.data(), &produced,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            static_cast<int>(plaintext.size())) != 1 ||
        EVP_EncryptFinal_ex(cipher.get(), ciphertext.data() + produced,
                            &final_bytes) != 1 ||
        EVP_CIPHER_CTX_ctrl(cipher.get(), EVP_CTRL_GCM_GET_TAG,
                            static_cast<int>(tag.size()), tag.data()) != 1) {
        throw std::runtime_error("Could not encrypt patient data.");
    }
    ciphertext.resize(static_cast<std::size_t>(produced + final_bytes));
#endif

    std::vector<unsigned char> envelope;
    envelope.reserve(nonce.size() + ciphertext.size() + tag.size());
    envelope.insert(envelope.end(), nonce.begin(), nonce.end());
    envelope.insert(envelope.end(), ciphertext.begin(), ciphertext.end());
    envelope.insert(envelope.end(), tag.begin(), tag.end());
    return envelope;
}

std::string decrypt_value(const std::string& encoded,
                          const std::string& key,
                          const std::string& context)
{
    std::vector<unsigned char> envelope;
    if (!decode_hex(encoded, envelope) ||
        envelope.size() < GCM_NONCE_SIZE + GCM_TAG_SIZE) {
        throw std::runtime_error("Stored patient data is not valid ciphertext.");
    }
    const std::size_t ciphertext_size =
        envelope.size() - GCM_NONCE_SIZE - GCM_TAG_SIZE;
    const unsigned char* nonce = envelope.data();
    const unsigned char* ciphertext = envelope.data() + GCM_NONCE_SIZE;
    const unsigned char* tag = ciphertext + ciphertext_size;
    std::string plaintext(ciphertext_size, '\0');

#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE aes_key = nullptr;
    std::vector<unsigned char> key_object;
    DWORD object_size = 0;
    DWORD bytes_read = 0;
    DWORD result_size = 0;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status >= 0) {
        status = BCryptSetProperty(
            algorithm, BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(
                BCRYPT_CHAIN_MODE_GCM)),
            static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)), 0);
    }
    if (status >= 0) {
        status = BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
            &bytes_read, 0);
    }
    if (status >= 0) {
        key_object.resize(object_size);
        status = BCryptGenerateSymmetricKey(
            algorithm, &aes_key, key_object.data(), object_size,
            reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
            static_cast<ULONG>(key.size()), 0);
    }
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = const_cast<PUCHAR>(nonce);
    auth_info.cbNonce = static_cast<ULONG>(GCM_NONCE_SIZE);
    auth_info.pbTag = const_cast<PUCHAR>(tag);
    auth_info.cbTag = static_cast<ULONG>(GCM_TAG_SIZE);
    auth_info.pbAuthData = reinterpret_cast<PUCHAR>(
        const_cast<char*>(context.data()));
    auth_info.cbAuthData = static_cast<ULONG>(context.size());
    if (status >= 0) {
        status = BCryptDecrypt(
            aes_key, const_cast<PUCHAR>(ciphertext),
            static_cast<ULONG>(ciphertext_size), &auth_info, nullptr, 0,
            reinterpret_cast<PUCHAR>(plaintext.data()),
            static_cast<ULONG>(plaintext.size()), &result_size, 0);
    }
    if (aes_key != nullptr) BCryptDestroyKey(aes_key);
    if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0 || result_size != plaintext.size()) {
        throw std::runtime_error(
            "Patient data authentication failed; check the encryption key.");
    }
#else
    using CipherContext = std::unique_ptr<EVP_CIPHER_CTX,
                                          decltype(&EVP_CIPHER_CTX_free)>;
    CipherContext cipher(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int produced = 0;
    int final_bytes = 0;
    if (!cipher ||
        EVP_DecryptInit_ex(cipher.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                           nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(cipher.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(GCM_NONCE_SIZE), nullptr) != 1 ||
        EVP_DecryptInit_ex(
            cipher.get(), nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.data()), nonce) != 1 ||
        EVP_DecryptUpdate(
            cipher.get(), nullptr, &produced,
            reinterpret_cast<const unsigned char*>(context.data()),
            static_cast<int>(context.size())) != 1 ||
        EVP_DecryptUpdate(
            cipher.get(), reinterpret_cast<unsigned char*>(plaintext.data()),
            &produced, ciphertext, static_cast<int>(ciphertext_size)) != 1 ||
        EVP_CIPHER_CTX_ctrl(cipher.get(), EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(GCM_TAG_SIZE),
                            const_cast<unsigned char*>(tag)) != 1 ||
        EVP_DecryptFinal_ex(
            cipher.get(),
            reinterpret_cast<unsigned char*>(plaintext.data()) + produced,
            &final_bytes) != 1) {
        throw std::runtime_error(
            "Patient data authentication failed; check the encryption key.");
    }
    plaintext.resize(static_cast<std::size_t>(produced + final_bytes));
#endif
    return plaintext;
}

std::string encrypt_hex(const std::string& value, const std::string& key,
                        const std::string& context)
{
    const auto encrypted = encrypt_value(value, key, context);
    return encode_hex(encrypted.data(), encrypted.size());
}

std::string prefix_array_sql(std::size_t count, std::size_t first_parameter)
{
    std::ostringstream sql;
    sql << "ARRAY[";
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) sql << ',';
        sql << "decode($" << first_parameter + index << ",'hex')";
    }
    sql << "]::bytea[]";
    return sql.str();
}

std::vector<const char*> pointers(const std::vector<std::string>& values)
{
    std::vector<const char*> result;
    result.reserve(values.size());
    for (const auto& value : values) result.push_back(value.c_str());
    return result;
}

std::string history_payload(const Registration& registration)
{
    std::ostringstream payload;
    const std::string values[] = {
        registration.name, registration.date_of_birth, registration.gender,
        registration.phone, registration.address
    };
    for (const auto& value : values) {
        payload << value.size() << ':' << value;
    }
    return payload.str();
}

bool parse_history_payload(const std::string& payload,
                           Registration& registration)
{
    std::string* fields[] = {
        &registration.name, &registration.date_of_birth,
        &registration.gender, &registration.phone, &registration.address
    };
    std::size_t position = 0;
    for (std::string* field : fields) {
        const std::size_t separator = payload.find(':', position);
        if (separator == std::string::npos || separator == position) {
            return false;
        }

        std::size_t length = 0;
        for (std::size_t index = position; index < separator; ++index) {
            const char digit = payload[index];
            if (digit < '0' || digit > '9') {
                return false;
            }
            const std::size_t value = static_cast<std::size_t>(digit - '0');
            if (length > (std::numeric_limits<std::size_t>::max() - value) /
                             10) {
                return false;
            }
            length = length * 10 + value;
        }

        position = separator + 1;
        if (length > payload.size() - position) {
            return false;
        }
        field->assign(payload, position, length);
        position += length;
    }
    return position == payload.size();
}

} // namespace

struct PostgresPatientRepository::Connection {
    PGconn* value = nullptr;
};

PostgresPatientRepository::PostgresPatientRepository(
    std::string data_encryption_key,
    const char* connection_info)
    : connection_(new Connection),
      data_encryption_key_(std::move(data_encryption_key))
{
    if (data_encryption_key_.size() != AES_KEY_SIZE) {
        delete connection_;
        connection_ = nullptr;
        throw std::invalid_argument(
            "HMS_PATIENT_DATA_KEY must contain exactly 32 bytes.");
    }

    try {
        connection_->value = pg_connectdb(
            connection_info == nullptr ? "" : connection_info);
    } catch (...) {
        delete connection_;
        connection_ = nullptr;
        throw;
    }
    if (connection_->value == nullptr ||
        pg_status(connection_->value) != CONNECTION_OK) {
        const std::string details = connection_->value == nullptr
            ? "Could not allocate PostgreSQL connection."
            : pg_error_message(connection_->value);
        if (connection_->value != nullptr) pg_finish(connection_->value);
        delete connection_;
        connection_ = nullptr;
        throw std::runtime_error("PostgreSQL connection failed: " + details);
    }
}

PostgresPatientRepository::~PostgresPatientRepository()
{
    if (connection_ != nullptr) {
        pg_finish(connection_->value);
        delete connection_;
    }
}

Patient PostgresPatientRepository::create_patient(
    const Registration& registration,
    const std::vector<BlindIndex>& name_prefix_indexes,
    const BlindIndex& phone_index)
{
    if (name_prefix_indexes.empty()) {
        throw std::invalid_argument("Patient name must not be empty.");
    }
    PGresult* sequence_raw = pg_exec_params(
        connection_->value,
        "SELECT nextval('patient_id_sequence')::text", 0, nullptr);
    require_status(connection_->value, sequence_raw, PGRES_TUPLES_OK,
                   "Could not generate patient ID");
    Result sequence_result(sequence_raw);
    const std::string sequence_value =
        pg_value(sequence_result.get(), 0, 0);
    if (sequence_value.empty() || sequence_value.size() > 8) {
        throw std::overflow_error("Patient ID range is exhausted.");
    }
    const std::string patient_id =
        "PID-" + std::string(8 - sequence_value.size(), '0') + sequence_value;

    std::vector<std::string> values = {
        patient_id,
        encrypt_hex(registration.name, data_encryption_key_, "patient:name:v1"),
        encrypt_hex(registration.date_of_birth, data_encryption_key_,
                    "patient:dob:v1"),
        registration.gender,
        encrypt_hex(registration.phone, data_encryption_key_,
                    "patient:phone:v1"),
        encrypt_hex(registration.address, data_encryption_key_,
                    "patient:address:v1"),
        registration.linked_user_id
    };
    values.reserve(8 + name_prefix_indexes.size());
    for (const auto& index : name_prefix_indexes) {
        values.push_back(encode_hex(index.data(), index.size()));
    }
    values.push_back(encode_hex(phone_index.data(), phone_index.size()));

    const std::string array = prefix_array_sql(name_prefix_indexes.size(), 8);
    const std::string sql =
        "INSERT INTO patients (patient_id,name,dob,gender,phone,address,user_id,"
        "name_prefix_blind_indexes,phone_blind_index) "
        "VALUES ($1,decode($2,'hex'),decode($3,'hex'),$4,"
        "decode($5,'hex'),decode($6,'hex'),NULLIF($7,'')::bigint," +
        array + ",decode($" + std::to_string(8 + name_prefix_indexes.size()) +
        ",'hex')) ON CONFLICT (user_id) DO NOTHING RETURNING patient_id";
    const auto params = pointers(values);
    PGresult* raw = pg_exec_params(
        connection_->value, sql.c_str(), static_cast<int>(params.size()),
        params.data());
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not register patient");
    Result result(raw);
    if (pg_tuples(result.get()) == 0) {
        return {};
    }
    return {pg_value(result.get(), 0, 0), registration};
}

bool PostgresPatientRepository::update_patient(
    const std::string& patient_id,
    const Registration& registration,
    const std::vector<BlindIndex>& name_prefix_indexes,
    const BlindIndex& phone_index,
    const std::string& changed_by_user_id)
{
    if (name_prefix_indexes.empty()) {
        throw std::invalid_argument("Patient name must not be empty.");
    }
    PGresult* begin = pg_exec_params(connection_->value, "BEGIN", 0, nullptr);
    require_status(connection_->value, begin, PGRES_COMMAND_OK,
                   "Could not start patient update");
    Result begin_result(begin);
    bool committed = false;
    try {
        const char* id_value[] = {patient_id.c_str()};
        PGresult* raw = pg_exec_params(
            connection_->value,
            "SELECT encode(name,'hex'),encode(dob,'hex'),gender,"
            "encode(phone,'hex'),encode(address,'hex') "
            "FROM patients WHERE patient_id=$1 FOR UPDATE",
            1, id_value);
        require_status(connection_->value, raw, PGRES_TUPLES_OK,
                       "Could not load patient for update");
        Result existing(raw);
        if (pg_tuples(existing.get()) == 0) {
            PGresult* rollback =
                pg_exec_params(connection_->value, "ROLLBACK", 0, nullptr);
            require_status(connection_->value, rollback, PGRES_COMMAND_OK,
                           "Could not roll back patient update");
            Result rollback_result(rollback);
            committed = true;
            return false;
        }
        Registration previous{
            decrypt_value(pg_value(existing.get(), 0, 0), data_encryption_key_,
                          "patient:name:v1"),
            decrypt_value(pg_value(existing.get(), 0, 1), data_encryption_key_,
                          "patient:dob:v1"),
            pg_value(existing.get(), 0, 2),
            decrypt_value(pg_value(existing.get(), 0, 3), data_encryption_key_,
                          "patient:phone:v1"),
            decrypt_value(pg_value(existing.get(), 0, 4), data_encryption_key_,
                          "patient:address:v1")
        };
        const std::string encrypted_history = encrypt_hex(
            history_payload(previous), data_encryption_key_,
            "patient:history:v1");
        const char* history_values[] = {
            patient_id.c_str(), encrypted_history.c_str(),
            changed_by_user_id.empty() ? nullptr : changed_by_user_id.c_str()
        };
        raw = pg_exec_params(
            connection_->value,
            "INSERT INTO patient_history(patient_id,old_values,changed_by) "
            "VALUES($1,decode($2,'hex'),$3::bigint)",
            3, history_values);
        require_status(connection_->value, raw, PGRES_COMMAND_OK,
                       "Could not record patient history");
        {
            Result history_result(raw);
        }

        std::vector<std::string> values = {
            patient_id,
            encrypt_hex(registration.name, data_encryption_key_,
                        "patient:name:v1"),
            encrypt_hex(registration.date_of_birth, data_encryption_key_,
                        "patient:dob:v1"),
            registration.gender,
            encrypt_hex(registration.phone, data_encryption_key_,
                        "patient:phone:v1"),
            encrypt_hex(registration.address, data_encryption_key_,
                        "patient:address:v1")
        };
        values.reserve(7 + name_prefix_indexes.size());
        for (const auto& index : name_prefix_indexes) {
            values.push_back(encode_hex(index.data(), index.size()));
        }
        values.push_back(encode_hex(phone_index.data(), phone_index.size()));

        const std::string array = prefix_array_sql(name_prefix_indexes.size(), 7);
        const std::string sql =
            "UPDATE patients SET name=decode($2,'hex'),dob=decode($3,'hex'),"
            "gender=$4,phone=decode($5,'hex'),address=decode($6,'hex'),"
            "name_prefix_blind_indexes=" + array +
            ",phone_blind_index=decode($" +
            std::to_string(7 + name_prefix_indexes.size()) +
            ",'hex'),updated_at=CURRENT_TIMESTAMP WHERE patient_id=$1";
        const auto params = pointers(values);
        raw = pg_exec_params(
            connection_->value, sql.c_str(), static_cast<int>(params.size()),
            params.data());
        require_status(connection_->value, raw, PGRES_COMMAND_OK,
                       "Could not update patient");
        {
            Result update_result(raw);
        }
        raw = pg_exec_params(connection_->value, "COMMIT", 0, nullptr);
        require_status(connection_->value, raw, PGRES_COMMAND_OK,
                       "Could not commit patient update");
        Result commit_result(raw);
        committed = true;
        return true;
    } catch (...) {
        if (!committed) {
            PGresult* rollback =
                pg_exec_params(connection_->value, "ROLLBACK", 0, nullptr);
            if (rollback != nullptr) pg_clear(rollback);
        }
        throw;
    }
}

namespace {

std::vector<Patient> read_patients(PGconn* connection,
                                   const std::string& sql,
                                   const std::vector<std::string>& values,
                                   const std::string& key)
{
    const auto params = pointers(values);
    PGresult* raw = pg_exec_params(
        connection, sql.c_str(), static_cast<int>(params.size()),
        params.empty() ? nullptr : params.data());
    require_status(connection, raw, PGRES_TUPLES_OK,
                   "Could not search patients");
    Result result(raw);
    std::vector<Patient> patients;
    patients.reserve(static_cast<std::size_t>(pg_tuples(result.get())));
    for (int row = 0; row < pg_tuples(result.get()); ++row) {
        patients.push_back({
            pg_value(result.get(), row, 0),
            {
                decrypt_value(pg_value(result.get(), row, 1), key,
                              "patient:name:v1"),
                decrypt_value(pg_value(result.get(), row, 2), key,
                              "patient:dob:v1"),
                pg_value(result.get(), row, 3),
                decrypt_value(pg_value(result.get(), row, 4), key,
                              "patient:phone:v1"),
                decrypt_value(pg_value(result.get(), row, 5), key,
                              "patient:address:v1")
            }
            ,pg_value(result.get(), row, 6)
        });
    }
    return patients;
}

} // namespace

std::vector<Patient> PostgresPatientRepository::find_by_id(
    const std::string& patient_id) const
{
    return read_patients(
        connection_->value,
        "SELECT patient_id,encode(name,'hex'),encode(dob,'hex'),gender,"
        "encode(phone,'hex'),encode(address,'hex'),"
        "COALESCE(user_id::text,'') "
        "FROM patients WHERE patient_id=$1",
        {patient_id}, data_encryption_key_);
}

std::vector<Patient> PostgresPatientRepository::find_by_name_prefix(
    const BlindIndex& name_prefix_index) const
{
    return read_patients(
        connection_->value,
        "SELECT patient_id,encode(name,'hex'),encode(dob,'hex'),gender,"
        "encode(phone,'hex'),encode(address,'hex'),"
        "COALESCE(user_id::text,'') "
        "FROM patients WHERE name_prefix_blind_indexes "
        "@> ARRAY[decode($1,'hex')]::bytea[] ORDER BY patient_id",
        {encode_hex(name_prefix_index.data(), name_prefix_index.size())},
        data_encryption_key_);
}

std::vector<Patient> PostgresPatientRepository::find_by_phone_index(
    const BlindIndex& phone_index) const
{
    return read_patients(
        connection_->value,
        "SELECT patient_id,encode(name,'hex'),encode(dob,'hex'),gender,"
        "encode(phone,'hex'),encode(address,'hex'),"
        "COALESCE(user_id::text,'') "
        "FROM patients WHERE phone_blind_index=decode($1,'hex') "
        "ORDER BY patient_id",
        {encode_hex(phone_index.data(), phone_index.size())},
        data_encryption_key_);
}

std::vector<PatientHistoryEntry> PostgresPatientRepository::patient_history(
    const std::string& patient_id) const
{
    const char* values[] = {patient_id.c_str()};
    PGresult* raw = pg_exec_params(
        connection_->value,
        "SELECT encode(h.old_values,'hex'),"
        "to_char(h.changed_at AT TIME ZONE 'UTC',"
        "'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'),"
        "COALESCE(u.username,'Deleted user') "
        "FROM patient_history h LEFT JOIN users u "
        "ON u.user_id=h.changed_by WHERE h.patient_id=$1 "
        "ORDER BY h.changed_at DESC,h.history_id DESC",
        1, values);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not read patient history");
    Result result(raw);

    std::vector<PatientHistoryEntry> entries;
    entries.reserve(static_cast<std::size_t>(pg_tuples(result.get())));
    for (int row = 0; row < pg_tuples(result.get()); ++row) {
        Registration old_values;
        const std::string payload = decrypt_value(
            pg_value(result.get(), row, 0), data_encryption_key_,
            "patient:history:v1");
        if (!parse_history_payload(payload, old_values)) {
            throw std::runtime_error(
                "Stored patient history has an invalid format.");
        }
        entries.push_back({
            std::move(old_values),
            pg_value(result.get(), row, 1),
            pg_value(result.get(), row, 2)
        });
    }
    return entries;
}

} // namespace hms::patients
