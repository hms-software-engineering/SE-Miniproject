#include "../src/billing/billing_service.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <libpq-fe.h>
#endif

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::cerr << "FAILED: " #condition " (line " << __LINE__        \
                      << ")\n";                                             \
            std::exit(1);                                                   \
        }                                                                   \
    } while (false)

namespace {

#ifndef _WIN32
struct VisitFixture {
    long long visit_id;
    long long appointment_id;
};

PGresult* run_query(PGconn* connection, const char* sql)
{
    PGresult* result = PQexec(connection, sql);
    CHECK(result != nullptr);
    if (PQresultStatus(result) != PGRES_COMMAND_OK &&
        PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::cerr << "PostgreSQL test query failed: "
                  << PQresultErrorMessage(result) << '\n';
        PQclear(result);
        std::exit(1);
    }
    return result;
}

VisitFixture create_visit_fixture(PGconn* connection)
{
    PGresult* result = run_query(
        connection,
        "WITH appointment AS ("
        "  INSERT INTO appointments "
        "    (patient_id, doctor_id, slot_start, status) "
        "  VALUES ("
        "    'PID-00000001', 1,"
        "    date_trunc('hour', CURRENT_TIMESTAMP) + INTERVAL '3 days' + "
        "      CASE WHEN EXTRACT(MINUTE FROM CURRENT_TIMESTAMP) < 30 "
        "        THEN INTERVAL '30 minutes' ELSE INTERVAL '1 hour' END,"
        "    'Completed')"
        "  RETURNING appointment_id"
        "), visit AS ("
        "  INSERT INTO visits "
        "    (appointment_id, symptoms, diagnosis, notes, doctor_id) "
        "  SELECT appointment_id, decode('01', 'hex'), "
        "    decode('02', 'hex'), decode('03', 'hex'), 1 FROM appointment "
        "  RETURNING visit_id, appointment_id"
        ") SELECT visit_id::text, appointment_id::text FROM visit");
    const VisitFixture fixture{
        std::stoll(PQgetvalue(result, 0, 0)),
        std::stoll(PQgetvalue(result, 0, 1))};
    PQclear(result);
    return fixture;
}

void test_postgres_repository_when_enabled()
{
    const char* enabled = std::getenv("HMS_BILLING_TEST_POSTGRES");
    if (enabled == nullptr || std::string(enabled) != "1") return;

    bool connection_rejected = false;
    try {
        hms::billing::PostgresBillingRepository invalid(
            "host=127.0.0.1 port=1 connect_timeout=1");
    } catch (const std::runtime_error&) {
        connection_rejected = true;
    }
    CHECK(connection_rejected);

    PGconn* connection = PQconnectdb("");
    CHECK(connection != nullptr);
    CHECK(PQstatus(connection) == CONNECTION_OK);

    const VisitFixture first = create_visit_fixture(connection);
    const VisitFixture failed_item = create_visit_fixture(connection);
    const std::string visits =
        std::to_string(first.visit_id) + "," +
        std::to_string(failed_item.visit_id);
    const std::string appointments =
        std::to_string(first.appointment_id) + "," +
        std::to_string(failed_item.appointment_id);

    {
        hms::billing::PostgresBillingRepository repository;
        const auto created = repository.persist_bill(
            first.visit_id, {{"Consultation", 100}}, 100, 0, 0, 100);
        CHECK(created.status == hms::billing::CreateBillStatus::Created);
        CHECK(created.bill.id > 0);
        CHECK(repository.persist_bill(
                  first.visit_id, {{"Consultation", 100}}, 100, 0, 0, 100)
                  .status == hms::billing::CreateBillStatus::AlreadyExists);
        CHECK(repository.persist_bill(
                  9223372036854775807LL,
                  {{"Consultation", 100}}, 100, 0, 0, 100).status ==
              hms::billing::CreateBillStatus::VisitMissing);

        bool item_rejected = false;
        try {
            repository.persist_bill(
                failed_item.visit_id,
                {{std::string(201, 'x'), 100}}, 100, 0, 0, 100);
        } catch (const std::runtime_error&) {
            item_rejected = true;
        }
        CHECK(item_rejected);
    }

    const std::string remove_items =
        "DELETE FROM bill_items WHERE bill_id IN "
        "(SELECT bill_id FROM bills WHERE visit_id IN (" + visits + "))";
    PGresult* result = run_query(connection, remove_items.c_str());
    PQclear(result);
    const std::string remove_bills =
        "DELETE FROM bills WHERE visit_id IN (" + visits + ")";
    result = run_query(connection, remove_bills.c_str());
    PQclear(result);
    const std::string remove_visits =
        "DELETE FROM visits WHERE visit_id IN (" + visits + ")";
    result = run_query(connection, remove_visits.c_str());
    PQclear(result);
    const std::string remove_appointments =
        "DELETE FROM appointments WHERE appointment_id IN (" + appointments + ")";
    result = run_query(connection, remove_appointments.c_str());
    PQclear(result);
    PQfinish(connection);
}
#else
void test_postgres_repository_when_enabled() {}
#endif

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

class FakePaymentRepository final : public hms::billing::PaymentRepository {
public:
    hms::billing::RecordPaymentResult persist_payment(
        long long bill_id,
        std::int64_t amount_cents,
        const std::string& mode) override
    {
        constexpr std::int64_t total_cents = 14809;
        if (bill_id != 1) {
            return {hms::billing::RecordPaymentStatus::BillMissing, {}};
        }
        if (amount_cents > total_cents - paid_cents) {
            return {hms::billing::RecordPaymentStatus::Overpayment, {}};
        }
        paid_cents += amount_cents;
        const std::int64_t remaining = total_cents - paid_cents;
        const std::string status = remaining == 0
            ? "Paid" : "Partially Paid";
        return {hms::billing::RecordPaymentStatus::Recorded,
                {next_id++, bill_id, amount_cents, remaining, mode,
                 "HMS-000000000001", status}};
    }

    std::int64_t paid_cents = 0;
    long long next_id = 1;
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
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"Consultation", "not-money"}}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"Consultation", "1."}}, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, {{"Consultation", "1.00"}}, "invalid").status ==
          hms::billing::CreateBillStatus::TaxConfigurationInvalid);
    std::vector<hms::billing::BillItemInput> too_many_items(
        101, {"Consultation", "1.00"});
    CHECK(hms::billing::create_itemized_bill(
              repository, 1, too_many_items, "18").status ==
          hms::billing::CreateBillStatus::InvalidRequest);

    test_postgres_repository_when_enabled();

    FakePaymentRepository payments;
    const auto first_payment =
        hms::billing::record_payment(payments, 1, "100.00", "Cash");
    CHECK(first_payment.status ==
          hms::billing::RecordPaymentStatus::Recorded);
    CHECK(first_payment.payment.amount_cents == 10000);
    CHECK(first_payment.payment.remaining_cents == 4809);
    CHECK(first_payment.payment.bill_status == "Partially Paid");
    CHECK(hms::billing::payment_to_json(first_payment.payment).find(
              "\"receiptNo\":\"HMS-000000000001\"") != std::string::npos);
    CHECK(hms::billing::record_payment(
              payments, 1, "48.10", "UPI").status ==
          hms::billing::RecordPaymentStatus::Overpayment);
    const auto final_payment =
        hms::billing::record_payment(payments, 1, "48.09", "Card");
    CHECK(final_payment.status ==
          hms::billing::RecordPaymentStatus::Recorded);
    CHECK(final_payment.payment.remaining_cents == 0);
    CHECK(final_payment.payment.bill_status == "Paid");
    CHECK(hms::billing::record_payment(
              payments, 1, "0.00", "Cash").status ==
          hms::billing::RecordPaymentStatus::InvalidRequest);
    CHECK(hms::billing::record_payment(
              payments, 1, "1.00", "Bank Transfer").status ==
          hms::billing::RecordPaymentStatus::InvalidRequest);

    std::cout << "All billing service tests passed.\n";
    return 0;
}
