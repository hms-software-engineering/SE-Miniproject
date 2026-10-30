#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hms::billing {

struct BillItemInput {
    std::string description;
    std::string amount;
};

struct BillItem {
    std::string description;
    std::int64_t amount_cents = 0;
};

struct Bill {
    long long id = 0;
    long long visit_id = 0;
    std::vector<BillItem> items;
    std::int64_t subtotal_cents = 0;
    std::int64_t tax_rate_basis_points = 0;
    std::int64_t tax_cents = 0;
    std::int64_t total_cents = 0;
    std::string status;
};

enum class CreateBillStatus {
    Created,
    InvalidRequest,
    VisitMissing,
    AlreadyExists,
    TaxConfigurationInvalid
};

struct CreateBillResult {
    CreateBillStatus status = CreateBillStatus::InvalidRequest;
    Bill bill;
};

struct PatientBill {
    Bill bill;
    std::int64_t paid_cents = 0;
};

enum class PaymentStatus {
    Recorded,
    InvalidRequest,
    BillMissing,
    BillCancelled,
    Overpayment
};

struct Payment {
    long long id = 0;
    long long bill_id = 0;
    std::int64_t amount_cents = 0;
    std::string mode;
    std::string receipt_no;
};

struct RecordPaymentResult {
    PaymentStatus status = PaymentStatus::InvalidRequest;
    Payment payment;
    std::int64_t total_cents = 0;
    std::int64_t paid_cents = 0;
    std::string bill_status;
};

struct Receipt {
    std::string receipt_no;
    long long payment_id = 0;
    long long bill_id = 0;
    std::int64_t amount_cents = 0;
    std::string mode;
    std::string paid_at;              // DD-MM-YYYY HH:MM (IST)
    std::int64_t total_cents = 0;
    std::int64_t balance_cents = 0;   // balance right after this payment
    std::string bill_status;
};

class BillRepository {
public:
    virtual ~BillRepository() = default;
    virtual CreateBillResult persist_bill(
        long long visit_id,
        const std::vector<BillItem>& items,
        std::int64_t subtotal_cents,
        std::int64_t tax_rate_basis_points,
        std::int64_t tax_cents,
        std::int64_t total_cents) = 0;
    // Must check the balance and insert atomically so that concurrent
    // payments cannot overpay a bill.
    virtual RecordPaymentResult persist_payment(
        long long bill_id,
        std::int64_t amount_cents,
        const std::string& mode) = 0;
    virtual std::optional<Receipt> find_receipt(long long /*payment_id*/)
    {
        return std::nullopt;
    }
};

class PostgresBillingRepository final : public BillRepository {
public:
    explicit PostgresBillingRepository(const char* connection_info = nullptr);
    ~PostgresBillingRepository() override;

    PostgresBillingRepository(const PostgresBillingRepository&) = delete;
    PostgresBillingRepository& operator=(const PostgresBillingRepository&) =
        delete;

    CreateBillResult persist_bill(
        long long visit_id,
        const std::vector<BillItem>& items,
        std::int64_t subtotal_cents,
        std::int64_t tax_rate_basis_points,
        std::int64_t tax_cents,
        std::int64_t total_cents) override;
    RecordPaymentResult persist_payment(
        long long bill_id,
        std::int64_t amount_cents,
        const std::string& mode) override;
    std::optional<Receipt> find_receipt(long long payment_id) override;
    // Newest bill first.
    std::vector<PatientBill> bills_for_patient(const std::string& patient_id);

private:
    struct Connection;
    Connection* connection_;
};

CreateBillResult create_itemized_bill(
    BillRepository& repository,
    long long visit_id,
    const std::vector<BillItemInput>& items,
    const std::string& tax_rate_percent);

RecordPaymentResult record_payment(
    BillRepository& repository,
    long long bill_id,
    const std::string& amount,
    const std::string& mode);

// Decides a payment against a bill's current state, before anything is stored.
PaymentStatus check_payment(const std::string& bill_status,
                            std::int64_t total_cents,
                            std::int64_t paid_cents,
                            std::int64_t amount_cents);
std::string bill_status_after_payment(std::int64_t total_cents,
                                      std::int64_t paid_cents);

std::string format_money(std::int64_t amount_cents);
std::string bill_to_json(const Bill& bill);
std::string payment_to_json(const RecordPaymentResult& result);
std::string patient_bills_to_json(const std::vector<PatientBill>& bills);
std::string render_receipt_html(const Receipt& receipt);

}  // namespace hms::billing
