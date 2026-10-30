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

        // ---- persist_bill checks (single copy) ----
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

        using hms::billing::PaymentStatus;

        // ---- Partial payment via record_payment (main) ----
        const auto partial = hms::billing::record_payment(
            repository, created.bill.id, "0.40", "UPI");
        CHECK(partial.status == PaymentStatus::Recorded);
        CHECK(partial.bill_status == "Partially Paid");
        CHECK(partial.payment.receipt_no.rfind("RCPT-", 0) == 0);

        // ---- find_receipt for the partial payment (feature) ----
        const auto partial_receipt =
            repository.find_receipt(partial.payment.id);
        CHECK(partial_receipt.has_value());
        CHECK(partial_receipt->amount_cents == 40);
        CHECK(partial_receipt->mode == "UPI");
        CHECK(partial_receipt->bill_id == created.bill.id);
        CHECK(partial_receipt->receipt_no == partial.payment.receipt_no);
        CHECK(partial_receipt->balance_cents == 60);

        // ---- Unknown receipt returns nullopt (feature) ----
        CHECK(!repository.find_receipt(999999).has_value());

        // ---- Overpayment on a partially paid bill (main) ----
        CHECK(hms::billing::record_payment(
                  repository, created.bill.id, "0.61", "Cash").status ==
              PaymentStatus::Overpayment);

        // ---- Settle the bill (main) ----
        const auto settled = hms::billing::record_payment(
            repository, created.bill.id, "0.60", "Card");
        CHECK(settled.status == PaymentStatus::Recorded);
        CHECK(settled.bill_status == "Paid");
        CHECK(settled.payment.receipt_no != partial.payment.receipt_no);

        // ---- find_receipt for the settling payment (feature) ----
        const auto settled_receipt =
            repository.find_receipt(settled.payment.id);
        CHECK(settled_receipt.has_value());
        CHECK(settled_receipt->amount_cents == 60);
        CHECK(settled_receipt->mode == "Card");
        CHECK(settled_receipt->balance_cents == 0);

        // ---- Any payment on a fully paid bill is an overpayment (feature) ----
        CHECK(repository.persist_payment(created.bill.id, 1, "Cash").status ==
              PaymentStatus::Overpayment);

        // ---- Missing bill on both entry points (main + feature) ----
        CHECK(hms::billing::record_payment(
                  repository, 9223372036854775807LL, "1.00", "Cash").status ==
              PaymentStatus::BillMissing);
        CHECK(repository.persist_payment(
                  9223372036854775807LL, 1000, "UPI").status ==
              PaymentStatus::BillMissing);

        // ---- TC-CLN-05: only the patient's own bills are returned (main) ----
        const auto bills = repository.bills_for_patient("PID-00000001");
        CHECK(!bills.empty());
        bool found_bill = false;
        for (const auto& entry : bills) {
            if (entry.bill.id != created.bill.id) continue;
            found_bill = true;
            CHECK(entry.bill.status == "Paid");
            CHECK(entry.paid_cents == 100);
            CHECK(entry.bill.items.size() == 1);
            CHECK(entry.bill.items[0].description == "Consultation");
        }
        CHECK(found_bill);
        CHECK(repository.bills_for_patient("PID-99999999").empty());
        CHECK(hms::billing::patient_bills_to_json(bills).find(
                  "\"balance\":\"0.00\"") != std::string::npos);
    }

    // Cleanup order matters: payments → bill_items → bills → visits → appointments.
    const std::string remove_payments =
        "DELETE FROM payments WHERE bill_id IN "
        "(SELECT bill_id FROM bills WHERE visit_id IN (" + visits + "))";
    PGresult* result = run_query(connection, remove_payments.c_str());
    PQclear(result);

    const std::string remove_items =
        "DELETE FROM bill_items WHERE bill_id IN "
        "(SELECT bill_id FROM bills WHERE visit_id IN (" + visits + "))";
    result = run_query(connection, remove_items.c_str());
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

    // Applies the same rules as the PostgreSQL repository to one bill.
    hms::billing::RecordPaymentResult persist_payment(
        long long bill_id,
        std::int64_t amount_cents,
        const std::string& mode) override
    {
        if (bill_id != 1) {
            return {hms::billing::PaymentStatus::BillMissing, {}, 0, 0, {}};
        }
        const auto check = hms::billing::check_payment(
            bill_status_, 84000, paid_cents_, amount_cents);
        if (check != hms::billing::PaymentStatus::Recorded) {
            return {check, {}, 84000, paid_cents_, bill_status_};
        }
        paid_cents_ += amount_cents;
        bill_status_ =
            hms::billing::bill_status_after_payment(84000, paid_cents_);
        ++payments_;
        return {hms::billing::PaymentStatus::Recorded,
                {payments_, bill_id, amount_cents, mode,
                 "RCPT-" + std::to_string(payments_)},
                84000, paid_cents_, bill_status_};
    }

    void cancel_bill() { bill_status_ = "Cancelled"; }
    long long payments() const { return payments_; }

private:
    std::int64_t paid_cents_ = 0;
    long long payments_ = 0;
    std::string bill_status_ = "Unpaid";
};

// TC-BIL-02: on 840.00 pay 400 by UPI, reject 500, then settle with 440.
void test_record_payment()
{
    using hms::billing::PaymentStatus;
    using hms::billing::record_payment;
    FakeBillRepository repository;

    const auto first = record_payment(repository, 1, "400.00", "UPI");
    CHECK(first.status == PaymentStatus::Recorded);
    CHECK(first.bill_status == "Partially Paid");
    CHECK(hms::billing::payment_to_json(first).find(
              "\"balance\":\"440.00\"") != std::string::npos);

    const auto over = record_payment(repository, 1, "500.00", "Cash");
    CHECK(over.status == PaymentStatus::Overpayment);
    CHECK(repository.payments() == 1);

    const auto last = record_payment(repository, 1, "440.00", "Card");
    CHECK(last.status == PaymentStatus::Recorded);
    CHECK(last.bill_status == "Paid");
    CHECK(last.payment.receipt_no != first.payment.receipt_no);

    CHECK(record_payment(repository, 1, "0.01", "Cash").status ==
          PaymentStatus::Overpayment);
    CHECK(record_payment(repository, 1, "1.00", "Cheque").status ==
          PaymentStatus::InvalidRequest);
    CHECK(record_payment(repository, 1, "1.00", "Bank Transfer").status ==
          PaymentStatus::InvalidRequest);
    CHECK(record_payment(repository, 1, "0.00", "Cash").status ==
          PaymentStatus::InvalidRequest);
    CHECK(record_payment(repository, 1, "-1.00", "Cash").status ==
          PaymentStatus::InvalidRequest);
    CHECK(record_payment(repository, 0, "1.00", "Cash").status ==
          PaymentStatus::InvalidRequest);
    CHECK(record_payment(repository, 99, "1.00", "Cash").status ==
          PaymentStatus::BillMissing);

    FakeBillRepository cancelled;
    cancelled.cancel_bill();
    CHECK(record_payment(cancelled, 1, "1.00", "Cash").status ==
          PaymentStatus::BillCancelled);
}

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

    test_record_payment();
    test_postgres_repository_when_enabled();

    std::cout << "All billing service tests passed.\n";
    return 0;
}