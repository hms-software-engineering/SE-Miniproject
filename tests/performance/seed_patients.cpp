// NF-001: seeds synthetic patients through the real repository code,
// then writes tests/performance/search_queries.csv for JMeter.
// Env: HMS_PATIENT_DATA_KEY, HMS_PATIENT_NAME_INDEX_KEY,
//      HMS_PATIENT_PHONE_INDEX_KEY (same values the server uses),
//      PGDATABASE/PGUSER/PGHOST/PGPASSWORD for libpq.
// Usage: nf001_seed_patients [count=100000]
#include "patients/patient_service.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace hms::patients;

namespace {
const char* FIRST[] = {"Aarav","Vivaan","Aditya","Ananya","Diya","Isha","Kabir",
    "Meera","Neha","Rohan","Saanvi","Tanvi","Arjun","Priya","Riya","Sanjay",
    "Divya","Karan","Pooja","Varun"};
const char* LAST[] = {"Sharma","Patel","Gupta","Reddy","Nair","Iyer","Singh",
    "Kumar","Mehta","Joshi","Rao","Das","Bose","Menon","Shetty","Kulkarni",
    "Verma","Jain","Pillai","Hegde"};

std::string need(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v) throw std::runtime_error(std::string("Set ") + name);
    return v;
}

// Unique letters-only suffix (base 26, 4 letters = 456,976 values).
std::string suffix(std::size_t i) {
    std::string s(4, 'a');
    for (int p = 3; p >= 0; --p) { s[p] = static_cast<char>('a' + i % 26); i /= 26; }
    s[0] = static_cast<char>(std::toupper(s[0]));
    return s;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const std::size_t total =
            argc > 1 ? std::stoul(argv[1]) : 100000;
        const std::string name_key = need("HMS_PATIENT_NAME_INDEX_KEY");
        const std::string phone_key = need("HMS_PATIENT_PHONE_INDEX_KEY");
        PostgresPatientRepository repo(need("HMS_PATIENT_DATA_KEY"));

        std::ofstream csv("tests/performance/search_queries.csv");
        if (!csv) throw std::runtime_error("Run from the repo root.");

        std::size_t done = 0;
        for (std::size_t i = 0; i < total; ++i) {
            Registration r;
            r.name = std::string(FIRST[i % 20]) + " " + LAST[(i / 20) % 20] +
                     " " + suffix(i);
            r.date_of_birth = "1990-01-01";
            r.gender = "Other";
            r.phone = std::to_string(9000000000ULL + i * 7);
            r.address = "Test Street " + std::to_string(i % 1000);

            const Patient p = repo.create_patient(
                r, make_name_prefix_blind_indexes(r.name, name_key),
                make_phone_blind_index(r.phone, phone_key));
            if (p.patient_id.empty()) throw std::runtime_error("Insert failed");

            // Sample ~1,000 patients for the query file.
            if (i % 100 == 0) {
                csv << p.patient_id << '\n';          // exact ID
                csv << r.phone << '\n';               // exact phone
                csv << r.name << '\n';                // full name (1 match)
                if (i % 1000 == 0)                    // "First Last" (~250 matches)
                    csv << FIRST[i % 20] << ' ' << LAST[(i / 20) % 20] << '\n';
            }
            if (++done % 5000 == 0) std::cout << done << " inserted\n";
        }
        for (int i = 1; i <= 100; ++i) csv << "Zzqx" << i << "nomatch\n";
        csv << "PID-99999999\n" << "9999999999\n";   // unmatched ID / phone
        std::cout << "Done: " << done << " patients seeded.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}