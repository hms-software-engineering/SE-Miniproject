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

struct Bill {
    long long id = 0;
    long long visit_id = 0;
    std::string patient_id;
    std::vector<BillItem> items;
    Paise subtotal = 0;
    Paise tax = 0;
    Paise total = 0;
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
};

class InMemoryBillingRepository : public BillingRepository {
public:
    InMemoryBillingRepository() = default;
    void add_visit(long long visit_id, const std::string& patient_id);

    std::string visit_patient(long long visit_id) const override;
    bool has_bill_for_visit(long long visit_id) const override;
    bool find_bill(long long bill_id, Bill& out) const override;
    Bill insert_bill(const Bill& b) override;

private:
    std::map<long long, std::string> visits_;
    std::map<long long, Bill> bills_;
    long long next_bill_id_ = 1;
};

struct BillingResult {
    bool ok = false;
    int http_status = 0;
    std::string code, message, field;
    Bill bill;
    Paise balance = 0;
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

// 84000 -> "840.00"
std::string format_amount(Paise amount);
std::string bill_to_json(const Bill& b);

}  // namespace hms
