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

class CapturingBillRepository final : public hms::billing::BillRepository {
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

    hms::billing::RecordPaymentResult persist_payment(
        long long, std::int64_t, const std::string&) override
    {
        return {};
    }
};

}  // namespace

int main()
{
    CapturingBillRepository repository;
    const auto large_bill = hms::billing::create_itemized_bill(
        repository, 1, {{"Service", "5000000.00"}}, "100");
    CHECK(large_bill.status == hms::billing::CreateBillStatus::Created);
    CHECK(large_bill.bill.subtotal_cents == 500000000);
    CHECK(large_bill.bill.tax_cents == 500000000);
    CHECK(large_bill.bill.total_cents == 1000000000);

    const auto maximum_untaxed_bill = hms::billing::create_itemized_bill(
        repository, 1, {{"Service", "9999999999.99"}}, "0");
    CHECK(maximum_untaxed_bill.status ==
          hms::billing::CreateBillStatus::Created);
    CHECK(maximum_untaxed_bill.bill.total_cents == 999999999999LL);

    const auto maximum_taxed_bill = hms::billing::create_itemized_bill(
        repository, 1, {{"Service", "9999999999.99"}}, "100");
    CHECK(maximum_taxed_bill.status ==
          hms::billing::CreateBillStatus::InvalidRequest);

    std::cout << "All billing tax boundary tests passed.\n";
    return 0;
}
