#pragma once

#include <cstdint>
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

private:
    struct Connection;
    Connection* connection_;
};

CreateBillResult create_itemized_bill(
    BillRepository& repository,
    long long visit_id,
    const std::vector<BillItemInput>& items,
    const std::string& tax_rate_percent);

std::string format_money(std::int64_t amount_cents);
std::string bill_to_json(const Bill& bill);

}  // namespace hms::billing
