#include "clinical/clinical_service.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
struct pg_conn;
struct pg_result;
using PGconn = pg_conn;
using PGresult = pg_result;
#else
#include <libpq-fe.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#ifdef _WIN32
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hms::clinical {
namespace {

constexpr int CONNECTION_OK = 0;
constexpr int PGRES_COMMAND_OK = 1;
constexpr int PGRES_TUPLES_OK = 2;
constexpr std::size_t AES_KEY_SIZE = 32;
constexpr std::size_t NONCE_SIZE = 12;
constexpr std::size_t TAG_SIZE = 16;
constexpr char HEX[] = "0123456789abcdef";

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
        if (library != nullptr) FreeLibrary(library);
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
PGresult* pg_exec_params(PGconn* connection, const char* sql, int count,
                         const char* const* values)
{
    return postgres_api().exec_params(
        connection, sql, count, nullptr, values, nullptr, nullptr, 0);
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
PGresult* pg_exec_params(PGconn* connection, const char* sql, int count,
                         const char* const* values)
{
    return PQexecParams(connection, sql, count, nullptr, values, nullptr,
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

class PgResult {
public:
    explicit PgResult(PGresult* value) : value_(value) {}
    ~PgResult()
    {
        if (value_ != nullptr) pg_clear(value_);
    }
    PgResult(const PgResult&) = delete;
    PgResult& operator=(const PgResult&) = delete;
    PGresult* get() const { return value_; }

private:
    PGresult* value_;
};

void require_status(PGconn* connection, PGresult* result, int expected,
                    const char* operation)
{
    if (result == nullptr || pg_result_status(result) != expected) {
        const std::string message = result == nullptr
            ? pg_error_message(connection)
            : pg_result_error_message(result);
        if (result != nullptr) pg_clear(result);
        throw std::runtime_error(std::string(operation) + ": " + message);
    }
}

PGresult* execute(PGconn* connection, const char* sql,
                  const std::vector<std::string>& values)
{
    std::vector<const char*> parameters;
    parameters.reserve(values.size());
    for (const auto& value : values) parameters.push_back(value.c_str());
    return pg_exec_params(connection, sql,
                          static_cast<int>(parameters.size()),
                          parameters.empty() ? nullptr : parameters.data());
}

std::string encrypt_hex(const std::string& plaintext,
                        const std::string& key,
                        const std::string& associated_data)
{
    std::array<unsigned char, NONCE_SIZE> nonce{};
    if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) {
        throw std::runtime_error("Could not create visit encryption nonce.");
    }
    std::vector<unsigned char> ciphertext(plaintext.size() + TAG_SIZE);
    int written = 0;
    int final_written = 0;
    int aad_written = 0;
    std::array<unsigned char, TAG_SIZE> tag{};
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> context(
        EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr,
                           nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1 ||
        EVP_EncryptInit_ex(
            context.get(), nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.data()),
            nonce.data()) != 1 ||
        EVP_EncryptUpdate(
            context.get(), nullptr, &aad_written,
            reinterpret_cast<const unsigned char*>(associated_data.data()),
            static_cast<int>(associated_data.size())) != 1 ||
        EVP_EncryptUpdate(
            context.get(), ciphertext.data(), &written,
            reinterpret_cast<const unsigned char*>(plaintext.data()),
            static_cast<int>(plaintext.size())) != 1 ||
        EVP_EncryptFinal_ex(context.get(), ciphertext.data() + written,
                            &final_written) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG,
                            static_cast<int>(tag.size()), tag.data()) != 1) {
        throw std::runtime_error("Could not encrypt visit data.");
    }
    ciphertext.resize(static_cast<std::size_t>(written + final_written));
    std::string encoded;
    encoded.reserve((nonce.size() + ciphertext.size() + tag.size()) * 2);
    const auto append_hex = [&encoded](const unsigned char* bytes,
                                       std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            encoded.push_back(HEX[bytes[i] >> 4]);
            encoded.push_back(HEX[bytes[i] & 0x0f]);
        }
    };
    append_hex(nonce.data(), nonce.size());
    append_hex(ciphertext.data(), ciphertext.size());
    append_hex(tag.data(), tag.size());
    return encoded;
}

} // namespace

ClinicalService::ClinicalService(ClinicalRepository& repository)
    : repository_(repository)
{
}

VisitRecordResult ClinicalService::record_visit(
    const VisitInput& input, const std::string& doctor_user_id)
{
    if (input.appointment_id <= 0 || doctor_user_id.empty() ||
        input.symptoms.empty() || input.diagnosis.empty() ||
        input.symptoms.size() > 4000 || input.diagnosis.size() > 4000 ||
        input.notes.size() > 8000) {
        return {};
    }
    return repository_.record_visit(input, doctor_user_id);
}

bool ClinicalService::add_prescription(
    const PrescriptionInput& input, const std::string& doctor_user_id)
{
    if (input.visit_id <= 0 || doctor_user_id.empty() ||
        input.item.empty() || input.item.size() > 200 ||
        input.dosage.empty() || input.dosage.size() > 120 ||
        input.frequency.empty() || input.frequency.size() > 120 ||
        input.days <= 0 || input.days > 3650) {
        return false;
    }
    return repository_.add_prescription(input, doctor_user_id);
}

struct PostgresClinicalRepository::Connection {
    PGconn* value = nullptr;
};

PostgresClinicalRepository::PostgresClinicalRepository(
    std::string encryption_key, const char* connection_info)
    : connection_(new Connection), encryption_key_(std::move(encryption_key))
{
    if (encryption_key_.size() != AES_KEY_SIZE) {
        delete connection_;
        connection_ = nullptr;
        throw std::invalid_argument(
            "Visit encryption key must contain exactly 32 bytes.");
    }
    connection_->value =
        pg_connectdb(connection_info == nullptr ? "" : connection_info);
    if (connection_->value == nullptr ||
        pg_status(connection_->value) != CONNECTION_OK) {
        const std::string message = connection_->value == nullptr
            ? "Could not allocate PostgreSQL connection."
            : pg_error_message(connection_->value);
        if (connection_->value != nullptr) pg_finish(connection_->value);
        delete connection_;
        connection_ = nullptr;
        throw std::runtime_error("PostgreSQL connection failed: " + message);
    }
}

PostgresClinicalRepository::~PostgresClinicalRepository()
{
    if (connection_ != nullptr) {
        pg_finish(connection_->value);
        delete connection_;
    }
}

VisitRecordResult PostgresClinicalRepository::record_visit(
    const VisitInput& input, const std::string& doctor_user_id)
{
    const std::vector<std::string> values{
        std::to_string(input.appointment_id),
        doctor_user_id,
        encrypt_hex(input.symptoms, encryption_key_, "visit:symptoms:v1"),
        encrypt_hex(input.diagnosis, encryption_key_, "visit:diagnosis:v1"),
        encrypt_hex(input.notes, encryption_key_, "visit:notes:v1")
    };
    PGresult* raw = execute(
        connection_->value,
        "INSERT INTO visits (appointment_id, symptoms, diagnosis, notes, doctor_id) "
        "SELECT a.appointment_id, decode($3,'hex'), decode($4,'hex'), "
        "decode($5,'hex'), d.doctor_id FROM appointments a "
        "JOIN doctors d ON d.doctor_id = a.doctor_id "
        "WHERE a.appointment_id = $1::bigint AND d.user_id = $2::bigint "
        "AND a.status = 'Confirmed' "
        "ON CONFLICT (appointment_id) DO NOTHING "
        "RETURNING visit_id::text",
        values);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not record visit");
    PgResult result(raw);
    if (pg_tuples(result.get()) != 0) {
        return {VisitRecordStatus::Created,
                std::stoll(pg_value(result.get(), 0, 0))};
    }

    PGresult* status_raw = execute(
        connection_->value,
        "SELECT EXISTS (SELECT 1 FROM appointments a "
        "JOIN doctors d ON d.doctor_id = a.doctor_id "
        "WHERE a.appointment_id = $1::bigint AND d.user_id = $2::bigint "
        "AND a.status = 'Confirmed'), "
        "EXISTS (SELECT 1 FROM visits v "
        "JOIN appointments a ON a.appointment_id = v.appointment_id "
        "JOIN doctors d ON d.doctor_id = a.doctor_id "
        "WHERE a.appointment_id = $1::bigint AND d.user_id = $2::bigint)",
        {std::to_string(input.appointment_id), doctor_user_id});
    require_status(connection_->value, status_raw, PGRES_TUPLES_OK,
                   "Could not verify visit status");
    PgResult status_result(status_raw);
    if (pg_tuples(status_result.get()) != 0 &&
        std::string(pg_value(status_result.get(), 0, 0)) == "t" &&
        std::string(pg_value(status_result.get(), 0, 1)) == "t") {
        return {VisitRecordStatus::AlreadyRecorded, 0};
    }
    return {VisitRecordStatus::AppointmentUnavailable, 0};
}

bool PostgresClinicalRepository::add_prescription(
    const PrescriptionInput& input, const std::string& doctor_user_id)
{
    const std::vector<std::string> values{
        std::to_string(input.visit_id),
        doctor_user_id,
        input.item,
        input.dosage,
        input.frequency,
        std::to_string(input.days)
    };
    PGresult* raw = execute(
        connection_->value,
        "INSERT INTO prescriptions (visit_id, item, dosage, frequency, days) "
        "SELECT v.visit_id, $3, $4, $5, $6::integer FROM visits v "
        "JOIN doctors d ON d.doctor_id = v.doctor_id "
        "WHERE v.visit_id = $1::bigint AND d.user_id = $2::bigint "
        "RETURNING prescription_id",
        values);
    require_status(connection_->value, raw, PGRES_TUPLES_OK,
                   "Could not add prescription");
    PgResult result(raw);
    return pg_tuples(result.get()) != 0;
}

} // namespace hms::clinical
