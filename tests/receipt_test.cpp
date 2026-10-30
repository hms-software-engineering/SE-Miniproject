#include "../src/billing/billing_service.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::cerr << "FAILED: " #condition " (line " << __LINE__        \
                      << ")\n";                                             \
            std::exit(1);                                                   \
        }                                                                   \
    } while (false)

namespace {

using namespace hms::billing;

// Mirrors the Postgres rules: check_payment guards, ids are sequential,
// receipt numbers follow the RCPT-%08d format.
class FakeRepository final : public BillRepository {
public:
    std::int64_t total_cents = 84000;  // TC-BIL-03: Rs 840 bill
    std::vector<Payment> payments;

    CreateBillResult persist_bill(long long, const std::vector<BillItem>&,
                                  std::int64_t, std::int64_t, std::int64_t,
                                  std::int64_t) override
    {
        return {};
    }

    std::int64_t paid() const
    {
        std::int64_t sum = 0;
        for (const auto& p : payments) sum += p.amount_cents;
        return sum;
    }

    RecordPaymentResult persist_payment(long long bill_id,
                                        std::int64_t amount_cents,
                                        const std::string& mode) override
    {
        const auto already = paid();
        const auto check =
            check_payment("Unpaid", total_cents, already, amount_cents);
        if (check != PaymentStatus::Recorded) {
            return {check, {}, total_cents, already, "Unpaid"};
        }
        const long long id = static_cast<long long>(payments.size()) + 1;
        char number[32];
        std::snprintf(number, sizeof number, "RCPT-%08lld", id);
        Payment payment{id, bill_id, amount_cents, mode, number};
        payments.push_back(payment);
        const auto now_paid = already + amount_cents;
        return {PaymentStatus::Recorded, payment, total_cents, now_paid,
                bill_status_after_payment(total_cents, now_paid)};
    }

    std::optional<Receipt> find_receipt(long long payment_id) override
    {
        std::int64_t through = 0;
        for (const auto& p : payments) {
            through += p.amount_cents;
            if (p.id != payment_id) continue;
            Receipt r;
            r.receipt_no = p.receipt_no;
            r.payment_id = p.id;
            r.bill_id = p.bill_id;
            r.amount_cents = p.amount_cents;
            r.mode = p.mode;
            r.paid_at = "30-10-2026 10:15";
            r.total_cents = total_cents;
            r.balance_cents = total_cents - through;
            r.bill_status = bill_status_after_payment(total_cents, through);
            return r;
        }
        return std::nullopt;
    }
};

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

}  // namespace

int main()
{
    FakeRepository repo;

    // TC-BIL-03 precondition: TC-BIL-02(a) - Rs 400 by UPI on Rs 840 bill.
    const auto first = record_payment(repo, 7, "400.00", "UPI");
    CHECK(first.status == PaymentStatus::Recorded);
    CHECK(first.total_cents - first.paid_cents == 44000);

    const auto receipt = repo.find_receipt(first.payment.id);
    CHECK(receipt.has_value());
    CHECK(receipt->receipt_no == first.payment.receipt_no);
    CHECK(receipt->amount_cents == 40000);
    CHECK(receipt->mode == "UPI");
    CHECK(receipt->balance_cents == 44000);

    const std::string html = render_receipt_html(*receipt);
    CHECK(contains(html, "window.print()"));       // printable
    CHECK(contains(html, "RCPT-00000001"));        // unique receipt number
    CHECK(contains(html, "400.00"));               // amount
    CHECK(contains(html, "UPI"));                  // mode
    CHECK(contains(html, "30-10-2026 10:15"));     // date
    CHECK(contains(html, "440.00"));               // balance

    // Failed payment (overpayment) creates no receipt.
    const auto over = record_payment(repo, 7, "500.00", "Cash");
    CHECK(over.status == PaymentStatus::Overpayment);
    CHECK(repo.payments.size() == 1);
    CHECK(!repo.find_receipt(2).has_value());

    // Second payment gets its own receipt; the first one is unchanged.
    const auto second = record_payment(repo, 7, "440.00", "Card");
    CHECK(second.status == PaymentStatus::Recorded);
    CHECK(second.payment.receipt_no != first.payment.receipt_no);
    CHECK(repo.find_receipt(second.payment.id)->balance_cents == 0);
    CHECK(repo.find_receipt(first.payment.id)->balance_cents == 44000);

    CHECK(!repo.find_receipt(99).has_value());     // unknown payment

    Receipt hostile = *receipt;
    hostile.mode = "<script>";
    CHECK(!contains(render_receipt_html(hostile), "<script>"));

    std::cout << "All receipt tests passed.\n";
    return 0;
}