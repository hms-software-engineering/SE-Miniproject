#include "../src/billing/billing.h"

#include <cstdlib>
#include <iostream>

#define CHECK(c)                                                          \
    do {                                                                  \
        if (!(c)) {                                                       \
            std::cerr << "FAILED: " #c " (line " << __LINE__ << ")\n";    \
            std::exit(1);                                                 \
        }                                                                 \
    } while (0)

int main() {
    using hms::Role;
    hms::InMemoryBillingRepository repo;
    repo.add_visit(1, "PID-00000001");
    repo.add_visit(2, "PID-00000001");
    repo.add_visit(3, "PID-00000002");

    // TC-BIL-01: consultation 500 + procedure 300 at 5% -> 800.00 / 40.00 / 840.00.
    auto b = hms::generate_bill(repo, Role::BillingOfficer, 1,
                                {{"Consultation", 50000}, {"Procedure", 30000}}, 500,
                                "2026-10-20 10:00");
    CHECK(b.ok);
    CHECK(b.http_status == 201);
    CHECK(b.bill.subtotal == 80000);
    CHECK(b.bill.tax == 4000);
    CHECK(b.bill.total == 84000);
    CHECK(b.bill.status == "Unpaid");
    CHECK(hms::format_amount(b.bill.total) == "840.00");
    CHECK(hms::bill_to_json(b.bill).find("\"total\":840.00") != std::string::npos);

    // Tax rounds half up to the paisa.
    CHECK(hms::tax_for(1010, 500) == 51);   // 50.5 paise -> 51
    CHECK(hms::tax_for(1000, 1800) == 180);

    // Validation, duplicates and RBAC.
    CHECK(hms::generate_bill(repo, Role::BillingOfficer, 1, {{"X", 100}}, 500, "t").http_status == 409);
    CHECK(hms::generate_bill(repo, Role::BillingOfficer, 99, {{"X", 100}}, 500, "t").http_status == 404);
    CHECK(hms::generate_bill(repo, Role::BillingOfficer, 2, {}, 500, "t").field == "items");
    CHECK(hms::generate_bill(repo, Role::BillingOfficer, 2, {{"X", -1}}, 500, "t").field == "items");
    CHECK(hms::generate_bill(repo, Role::BillingOfficer, 2, {{"X", 1}}, -5, "t").field == "taxRate");
    CHECK(hms::generate_bill(repo, Role::Doctor, 2, {{"X", 100}}, 500, "t").message == "Access denied");

    std::cout << "All billing tests passed.\n";
    return 0;
}
