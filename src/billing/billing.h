#pragma once
#include <map>
#include <string>
#include <vector>

#include "services/rbac_service.h"

namespace hms {

// Money is held in paise (1/100 rupee) so totals never drift.
using Paise = long long;

struct BillItem {
    std::string item;
    Paise amount = 0;
};

struct Payment {
    long long id = 0;
    long long bill_id = 0;
    Paise amount = 0;
    std::string mode;        // "Cash", "Card" or "UPI"
    std::string receipt_no;  // unique per payment
    std::string paid_at;     // "YYYY-MM-DD HH:MM"
};

struct Bill {
    long long id = 0;
    long long visit_id = 0;
    std::string patient_id;
    std::vector<BillItem> items;
    Paise subtotal = 0;
    Paise tax = 0;
    Paise total = 0;
    Paise paid = 0;
    std::string status;  // "Unpaid", "Partially Paid" or "Paid"
    std::string created_at;
};

// A PostgreSQL version will implement this same interface later.
class BillingRepository {
public:
    virtual ~BillingRepository() = default;
    // Patient ID for a visit, or "" if the visit does not exist.
    virtual std::string visit_patient(long long visit_id) const = 0;
    virtual bool has_bill_for_visit(long long visit_id) const = 0;
    virtual bool find_bill(long long bill_id, Bill& out) const = 0;
    virtual Bill insert_bill(const Bill& b) = 0;
    // Must be atomic with the status update. The database version locks the bill row.
    virtual Payment insert_payment(const Payment& p, const std::string& new_status) = 0;
    virtual std::vector<Bill> bills_for_patient(const std::string& patient_id) const = 0;
    virtual std::vector<Payment> payments_for_bill(long long bill_id) const = 0;
};

class InMemoryBillingRepository : public BillingRepository {
public:
    InMemoryBillingRepository() = default;
    void add_visit(long long visit_id, const std::string& patient_id);

    std::string visit_patient(long long visit_id) const override;
    bool has_bill_for_visit(long long visit_id) const override;
    bool find_bill(long long bill_id, Bill& out) const override;
    Bill insert_bill(const Bill& b) override;
    Payment insert_payment(const Payment& p, const std::string& new_status) override;
    std::vector<Bill> bills_for_patient(const std::string& patient_id) const override;
    std::vector<Payment> payments_for_bill(long long bill_id) const override;

private:
    std::map<long long, std::string> visits_;
    std::map<long long, Bill> bills_;
    std::vector<Payment> payments_;
    long long next_bill_id_ = 1;
    long long next_payment_id_ = 1;
};

struct BillingResult {
    bool ok = false;
    int http_status = 0;
    std::string code, message, field;
    Bill bill;
    Payment payment;
    Paise balance = 0;
    std::vector<Bill> bills;
};

// Tax on a subtotal, rate in basis points (500 = 5%), rounded half up to the paisa.
Paise tax_for(Paise subtotal, int tax_rate_bp);

// HMS-F-040
BillingResult generate_bill(BillingRepository& repo,
                            Role caller,
                            long long visit_id,
                            const std::vector<BillItem>& items,
                            int tax_rate_bp,
                            const std::string& now);

// HMS-F-041: rejects any payment above the outstanding balance.
BillingResult record_payment(BillingRepository& repo,
                             Role caller,
                             long long bill_id,
                             Paise amount,
                             const std::string& mode,
                             const std::string& now);

// HMS-F-034: a patient's own bills, newest first.
BillingResult patient_bills(const BillingRepository& repo,
                            Role caller,
                            const std::string& caller_patient_id,
                            const std::string& patient_id);

// 84000 -> "840.00"
std::string format_amount(Paise amount);
std::string bill_to_json(const Bill& b);

}  // namespace hms
