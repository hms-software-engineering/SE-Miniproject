#include "billing/billing.h"

#include <algorithm>
#include <cstdio>

namespace hms {

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out;
}

BillingResult fail(int status, const std::string& code, const std::string& msg,
                   const std::string& field = "") {
    BillingResult r;
    r.http_status = status;
    r.code = code;
    r.message = msg;
    r.field = field;
    return r;
}

BillingResult access_denied() { return fail(403, "ACCESS_DENIED", "Access denied"); }

std::string status_for(Paise paid, Paise total) {
    if (paid <= 0) return "Unpaid";
    return paid >= total ? "Paid" : "Partially Paid";
}

}  // namespace

void InMemoryBillingRepository::add_visit(long long visit_id, const std::string& patient_id) {
    visits_[visit_id] = patient_id;
}

std::string InMemoryBillingRepository::visit_patient(long long visit_id) const {
    auto it = visits_.find(visit_id);
    return it == visits_.end() ? "" : it->second;
}

bool InMemoryBillingRepository::has_bill_for_visit(long long visit_id) const {
    for (const auto& kv : bills_)
        if (kv.second.visit_id == visit_id) return true;
    return false;
}

bool InMemoryBillingRepository::find_bill(long long bill_id, Bill& out) const {
    auto it = bills_.find(bill_id);
    if (it == bills_.end()) return false;
    out = it->second;
    return true;
}

Bill InMemoryBillingRepository::insert_bill(const Bill& b) {
    Bill saved = b;
    saved.id = next_bill_id_++;
    bills_[saved.id] = saved;
    return saved;
}

Payment InMemoryBillingRepository::insert_payment(const Payment& p, const std::string& new_status) {
    Payment saved = p;
    saved.id = next_payment_id_++;
    char buf[32];
    std::snprintf(buf, sizeof buf, "RCPT-%08lld", saved.id);
    saved.receipt_no = buf;
    payments_.push_back(saved);
    Bill& b = bills_[p.bill_id];
    b.paid += p.amount;
    b.status = new_status;
    return saved;
}

std::vector<Bill> InMemoryBillingRepository::bills_for_patient(const std::string& patient_id) const {
    std::vector<Bill> out;
    for (const auto& kv : bills_)
        if (kv.second.patient_id == patient_id) out.push_back(kv.second);
    return out;
}

std::vector<Payment> InMemoryBillingRepository::payments_for_bill(long long bill_id) const {
    std::vector<Payment> out;
    for (const auto& p : payments_)
        if (p.bill_id == bill_id) out.push_back(p);
    return out;
}

Paise tax_for(Paise subtotal, int tax_rate_bp) {
    return (subtotal * tax_rate_bp + 5000) / 10000;
}

BillingResult generate_bill(BillingRepository& repo,
                            Role caller,
                            long long visit_id,
                            const std::vector<BillItem>& items,
                            int tax_rate_bp,
                            const std::string& now) {
    std::string err;
    if (!authorize(caller, Permission::BillingPaymentsReceipts, err)) return access_denied();

    if (items.empty())
        return fail(400, "VALIDATION_ERROR", "At least one bill item is required", "items");
    if (tax_rate_bp < 0 || tax_rate_bp > 10000)
        return fail(400, "VALIDATION_ERROR", "Tax rate must be between 0% and 100%", "taxRate");

    Paise subtotal = 0;
    for (const auto& it : items) {
        if (it.item.find_first_not_of(" \t") == std::string::npos)
            return fail(400, "VALIDATION_ERROR", "Item name is required", "items");
        if (it.amount < 0)
            return fail(400, "VALIDATION_ERROR", "Item amount cannot be negative", "items");
        subtotal += it.amount;
    }

    std::string patient_id = repo.visit_patient(visit_id);
    if (patient_id.empty()) return fail(404, "NOT_FOUND", "Visit not found", "visitId");
    if (repo.has_bill_for_visit(visit_id))
        return fail(409, "BILL_EXISTS", "A bill already exists for this visit", "visitId");

    Bill b;
    b.visit_id = visit_id;
    b.patient_id = patient_id;
    b.items = items;
    b.subtotal = subtotal;
    b.tax = tax_for(subtotal, tax_rate_bp);
    b.total = b.subtotal + b.tax;
    b.status = status_for(0, b.total);
    // A zero-total bill is settled on creation.
    if (b.total == 0) b.status = "Paid";
    b.created_at = now;

    BillingResult r;
    r.ok = true;
    r.http_status = 201;
    r.bill = repo.insert_bill(b);
    r.balance = r.bill.total;
    return r;
}

BillingResult record_payment(BillingRepository& repo,
                             Role caller,
                             long long bill_id,
                             Paise amount,
                             const std::string& mode,
                             const std::string& now) {
    std::string err;
    if (!authorize(caller, Permission::BillingPaymentsReceipts, err)) return access_denied();

    if (mode != "Cash" && mode != "Card" && mode != "UPI")
        return fail(400, "VALIDATION_ERROR", "Mode must be Cash, Card or UPI", "mode");
    if (amount <= 0)
        return fail(400, "VALIDATION_ERROR", "Amount must be greater than zero", "amount");

    Bill b;
    if (!repo.find_bill(bill_id, b)) return fail(404, "NOT_FOUND", "Bill not found", "billId");

    const Paise outstanding = b.total - b.paid;
    if (amount > outstanding)
        return fail(400, "OVERPAYMENT",
                    "Payment exceeds the outstanding balance of " + format_amount(outstanding),
                    "amount");

    Payment p;
    p.bill_id = bill_id;
    p.amount = amount;
    p.mode = mode;
    p.paid_at = now;

    BillingResult r;
    r.ok = true;
    r.http_status = 201;
    r.payment = repo.insert_payment(p, status_for(b.paid + amount, b.total));
    repo.find_bill(bill_id, r.bill);
    r.balance = r.bill.total - r.bill.paid;
    return r;
}

BillingResult patient_bills(const BillingRepository& repo,
                            Role caller,
                            const std::string& caller_patient_id,
                            const std::string& patient_id) {
    std::string err;
    if (!authorize_record_access(caller, Permission::ViewOwnHistoryBills, caller_patient_id,
                                 patient_id, err))
        return access_denied();

    BillingResult r;
    r.ok = true;
    r.http_status = 200;
    r.bills = repo.bills_for_patient(patient_id);
    std::sort(r.bills.begin(), r.bills.end(), [](const Bill& a, const Bill& b) {
        if (a.created_at != b.created_at) return a.created_at > b.created_at;
        return a.id > b.id;
    });
    return r;
}

std::string format_amount(Paise amount) {
    const char* sign = amount < 0 ? "-" : "";
    if (amount < 0) amount = -amount;
    char buf[40];
    std::snprintf(buf, sizeof buf, "%s%lld.%02lld", sign, amount / 100, amount % 100);
    return buf;
}

std::string bill_to_json(const Bill& b) {
    std::string out = "{\"billId\":" + std::to_string(b.id) +
                      ",\"visitId\":" + std::to_string(b.visit_id) + ",\"items\":[";
    for (size_t i = 0; i < b.items.size(); ++i) {
        if (i) out += ",";
        out += "{\"item\":\"" + json_escape(b.items[i].item) +
               "\",\"amount\":" + format_amount(b.items[i].amount) + "}";
    }
    out += "],\"subtotal\":" + format_amount(b.subtotal) +
           ",\"tax\":" + format_amount(b.tax) +
           ",\"total\":" + format_amount(b.total) +
           ",\"balance\":" + format_amount(b.total - b.paid) +
           ",\"status\":\"" + b.status + "\"}";
    return out;
}

}  // namespace hms
