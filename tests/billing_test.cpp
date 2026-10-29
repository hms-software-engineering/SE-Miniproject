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

    // TC-BIL-02: on 840.00 pay (a) 400 UPI, (b) 500 rejected, (c) 440 -> Paid.
    const long long id = b.bill.id;
    auto a = hms::record_payment(repo, Role::BillingOfficer, id, 40000, "UPI", "2026-10-20 10:05");
    CHECK(a.ok);
    CHECK(a.bill.status == "Partially Paid");
    CHECK(a.balance == 44000);
    CHECK(!a.payment.receipt_no.empty());

    auto over = hms::record_payment(repo, Role::BillingOfficer, id, 50000, "Cash", "t");
    CHECK(!over.ok);
    CHECK(over.http_status == 400);
    CHECK(over.code == "OVERPAYMENT");
    CHECK(repo.payments_for_bill(id).size() == 1);

    auto c = hms::record_payment(repo, Role::BillingOfficer, id, 44000, "Card", "2026-10-20 10:10");
    CHECK(c.ok);
    CHECK(c.bill.status == "Paid");
    CHECK(c.balance == 0);
    CHECK(c.payment.receipt_no != a.payment.receipt_no);

    // Paid bill accepts nothing further; bad input is rejected.
    CHECK(hms::record_payment(repo, Role::BillingOfficer, id, 1, "Cash", "t").code == "OVERPAYMENT");
    CHECK(hms::record_payment(repo, Role::BillingOfficer, id, 1, "Cheque", "t").field == "mode");
    CHECK(hms::record_payment(repo, Role::BillingOfficer, id, 0, "Cash", "t").field == "amount");
    CHECK(hms::record_payment(repo, Role::BillingOfficer, 99, 1, "Cash", "t").http_status == 404);
    CHECK(hms::record_payment(repo, Role::Receptionist, id, 1, "Cash", "t").http_status == 403);

    // HMS-F-034: patient sees own bills newest first, not others'.
    hms::generate_bill(repo, Role::BillingOfficer, 2, {{"Consultation", 50000}}, 500, "2026-10-22 09:00");
    hms::generate_bill(repo, Role::BillingOfficer, 3, {{"Consultation", 50000}}, 500, "2026-10-23 09:00");
    auto own = hms::patient_bills(repo, Role::Patient, "PID-00000001", "PID-00000001");
    CHECK(own.ok);
    CHECK(own.bills.size() == 2);
    CHECK(own.bills[0].visit_id == 2);
    CHECK(hms::patient_bills(repo, Role::Patient, "PID-00000001", "PID-00000002").message ==
          "Access denied");
    CHECK(hms::patient_bills(repo, Role::Doctor, "", "PID-00000001").http_status == 403);

    std::cout << "All billing tests passed.\n";
    return 0;
}
