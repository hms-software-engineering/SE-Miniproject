#include "../src/billing/billing_service.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::cerr << "FAILED: " #condition " (line " << __LINE__        \
                      << ")\n";                                             \
            std::exit(1);                                                   \
        }                                                                   \
    } while (false)

namespace {

class FakeBillRepository final : public hms::billing::BillRepository {
public:
    hms::billing::CreateBillResult persist_bill(
        long long visit_id,
        const std::vector<hms::billing::BillItem>& items,
        std::int64_t subtotal_cents,
        std::int64_t tax_rate_basis_points,
        std::int64_t tax_cents,
        std::int64_t total_cents) override
    {
        return {hms::billing::CreateBillStatus::Created,
                {1, visit_id, items, subtotal_cents, tax_rate_basis_points,
                 tax_cents, total_cents, "Unpaid"}};
    }
};

}  // namespace

int main()
{
    FakeBillRepository repository;
    const auto bill = hms::billing::create_itemized_bill(
        repository, 17,
        {{"Consultation", "100.00"}, {"Medicine (2)", "25.50"}},
        "18");
    CHECK(bill.status == hms::billing::CreateBillStatus::Created);
    CHECK(bill.bill.visit_id == 17);
    CHECK(bill.bill.items.size() == 2);
    CHECK(bill.bill.subtotal_cents == 12550);
    CHECK(bill.bill.tax_rate_basis_points == 1800);
    CHECK(bill.bill.tax_cents == 2259);
    CHECK(bill.bill.total_cents == 14809);
    CHECK(hms::billing::format_money(bill.bill.total_cents) == "148.09");
    CHECK(hms::billing::bill_to_json(bill.bill).find(
              "\"description\":\"Medicine (2)\"") != std::string::npos);
    CHECK(hms::billing::bill_to_json(bill.bill).find(
              "\"taxRate\":\"18.00\"") != std::string::npos);

    const auto rounded = hms::billing::create_itemized_bill(
        repository, 1, {{"Rounding", "0.03"}}, "18");
    CHECK(rounded.bill.tax_cents == 1);
    CHECK(hms::billing::create_itemized_bill(
              repository, 0, {{"Consultation", "10.00"}}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"Consultation", "-1.00"}}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"Consultation", "1.001"}}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"Consultation", "10.00"}}, "100.01").status ==
          hms::billing::CreateBillStatus::TaxConfigurationInvalid);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"", "10.00"}}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"   ", "10.00"}}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);

    std::cout << "All billing service tests passed.\n";
    return 0;
}
