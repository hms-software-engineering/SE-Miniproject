#include "patients/field_crypto.h"
#include "patients/patient_service.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef HMS_TEST_HAS_LIBPQ
#include <libpq-fe.h>
#endif

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

std::string hex_of(const std::string& value)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : value) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
    }
    return out;
}

std::string flip(std::string hex, std::size_t position)
{
    hex[position] = hex[position] == '0' ? '1' : '0';
    return hex;
}

template <typename Fn>
bool throws(Fn fn)
{
    try { fn(); } catch (const std::exception&) { return true; }
    return false;
}

void test_crypto_unit()
{
    const std::string key(32, 'k');
    const std::string other_key(32, 'x');
    const std::string name = "Asha Nair";   // dummy data only
    const std::string ctx = "patient:name:v1";

    const std::string encrypted = encrypt_patient_field(name, key, ctx);
    CHECK(encrypted != name);
    CHECK(encrypted.find(name) == std::string::npos);
    CHECK(encrypted.find(hex_of(name)) == std::string::npos);
    CHECK(encrypted.size() >= 2 * (name.size() + 12 + 16));
    CHECK(decrypt_patient_field(encrypted, key, ctx) == name);

    // Fresh nonce each time -> different ciphertext for identical plaintext.
    CHECK(encrypted != encrypt_patient_field(name, key, ctx));

    // Empty value round-trips.
    CHECK(decrypt_patient_field(encrypt_patient_field("", key, ctx), key, ctx).empty());

    // Tampering with nonce, ciphertext body, or tag must fail.
    CHECK(throws([&] { decrypt_patient_field(flip(encrypted, 0), key, ctx); }));
    CHECK(throws([&] { decrypt_patient_field(flip(encrypted, 30), key, ctx); }));
    CHECK(throws([&] { decrypt_patient_field(flip(encrypted, encrypted.size() - 1), key, ctx); }));

    // Wrong key, wrong field context, truncated, non-hex, bad key size.
    CHECK(throws([&] { decrypt_patient_field(encrypted, other_key, ctx); }));
    CHECK(throws([&] { decrypt_patient_field(encrypted, key, "patient:phone:v1"); }));
    CHECK(throws([&] { decrypt_patient_field(encrypted.substr(0, 20), key, ctx); }));
    CHECK(throws([&] { decrypt_patient_field("zz" + encrypted.substr(2), key, ctx); }));
    CHECK(throws([&] { encrypt_patient_field(name, "short", ctx); }));
}

#ifdef HMS_TEST_HAS_LIBPQ
struct RawRow {
    std::vector<std::string> columns;  // name, dob, phone, address, allergies (hex)
    std::vector<std::string> history;  // old_values (hex)
};

RawRow read_raw(const std::string& patient_id)
{
    PGconn* conn = PQconnectdb("");
    CHECK(PQstatus(conn) == CONNECTION_OK);
    const char* params[] = {patient_id.c_str()};
    RawRow row;

    PGresult* r = PQexecParams(
        conn,
        "SELECT encode(name,'hex'),encode(dob,'hex'),encode(phone,'hex'),"
        "encode(address,'hex'),encode(allergies,'hex') "
        "FROM patients WHERE patient_id=$1",
        1, nullptr, params, nullptr, nullptr, 0);
    CHECK(PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) == 1);
    for (int c = 0; c < 5; ++c) {
        row.columns.push_back(PQgetisnull(r, 0, c) ? "" : PQgetvalue(r, 0, c));
    }
    PQclear(r);

    r = PQexecParams(conn,
        "SELECT encode(old_values,'hex') FROM patient_history WHERE patient_id=$1",
        1, nullptr, params, nullptr, nullptr, 0);
    CHECK(PQresultStatus(r) == PGRES_TUPLES_OK);
    for (int i = 0; i < PQntuples(r); ++i) row.history.push_back(PQgetvalue(r, i, 0));
    PQclear(r);
    PQfinish(conn);
    return row;
}

void check_is_ciphertext(const std::string& stored_hex, const std::string& plaintext)
{
    CHECK(stored_hex.size() >= 2 * (12 + 16));            // nonce + tag at minimum
    CHECK(stored_hex.find(hex_of(plaintext)) == std::string::npos);
}
#endif

void test_database_stores_ciphertext()
{
#ifdef HMS_TEST_HAS_LIBPQ
    const char* enabled = std::getenv("HMS_PATIENT_TEST_POSTGRES");
    if (enabled == nullptr || std::string(enabled) != "1") return;
    const char* data_key = std::getenv("HMS_PATIENT_DATA_KEY");
    const char* name_key = std::getenv("HMS_PATIENT_NAME_INDEX_KEY");
    const char* phone_key = std::getenv("HMS_PATIENT_PHONE_INDEX_KEY");
    CHECK(data_key && name_key && phone_key);

    PostgresPatientRepository repository(data_key);
    PatientService service(repository, name_key, phone_key);

    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    Registration reg{"Sec Six Test " + std::to_string(stamp % 1000000000),
                     "14-03-1990", "Female",
                     "9" + std::to_string(100000000 + stamp % 899999999),
                     "Dummy Address 12 Test Road"};
    reg.allergies = "Dummy Penicillin Allergy";

    const auto created = service.register_patient(reg, "21-10-2026");
    CHECK(created.saved);
    const std::string id = created.patient.patient_id;

    // TC-SEC-06: inspect the actual table, not the API.
    RawRow raw = read_raw(id);
    check_is_ciphertext(raw.columns[0], reg.name);
    check_is_ciphertext(raw.columns[1], reg.date_of_birth);
    check_is_ciphertext(raw.columns[2], reg.phone);
    check_is_ciphertext(raw.columns[3], reg.address);
    check_is_ciphertext(raw.columns[4], reg.allergies);

    // Retrieval returns original values, including allergies.
    auto found = service.search_patients(id);
    CHECK(found.matches.size() == 1);
    CHECK(found.matches.front().demographics.name == reg.name);
    CHECK(found.matches.front().demographics.allergies == reg.allergies);

    // Update: new values encrypted, history holds no plaintext.
    Registration updated = reg;
    updated.address = "Dummy Updated Address 99";
    updated.allergies = "Dummy Latex Allergy";
    CHECK(service.update_patient(id, updated, "21-10-2026", "1").saved);
    raw = read_raw(id);
    check_is_ciphertext(raw.columns[3], updated.address);
    check_is_ciphertext(raw.columns[4], updated.allergies);
    CHECK(raw.history.size() == 1);
    check_is_ciphertext(raw.history[0], reg.name);
    check_is_ciphertext(raw.history[0], reg.address);
    CHECK(service.search_patients(id).matches.front().demographics.allergies ==
          updated.allergies);

    // Clearing allergies stores NULL and reads back as empty.
    updated.allergies.clear();
    CHECK(service.update_patient(id, updated, "21-10-2026", "1").saved);
    CHECK(read_raw(id).columns[4].empty());
    CHECK(service.search_patients(id).matches.front().demographics.allergies.empty());
#endif
}

} // namespace

int main()
{
    test_crypto_unit();
    test_database_stores_ciphertext();
    std::cout << "patient_field_encryption_test passed\n";
    return 0;
}