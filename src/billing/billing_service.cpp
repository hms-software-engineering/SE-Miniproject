#include "billing/billing_service.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

struct pg_conn;
struct pg_result;
using PGconn = pg_conn;
using PGresult = pg_result;
#else
#include <libpq-fe.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace hms::billing {
namespace {

constexpr int CONNECTION_OK = 0;
constexpr int PGRES_COMMAND_OK = 1;
constexpr int PGRES_TUPLES_OK = 2;
constexpr std::int64_t MAX_AMOUNT_CENTS = 999999999999LL;

#ifdef _WIN32
struct PostgresApi {
    HMODULE library = nullptr;
    PGconn* (__cdecl* connectdb)(const char*) = nullptr;
    int (__cdecl* status)(const PGconn*) = nullptr;
    char* (__cdecl* error_message)(const PGconn*) = nullptr;
    void (__cdecl* finish)(PGconn*) = nullptr;
    PGresult* (__cdecl* exec_params)(
        PGconn*, const char*, int, const unsigned int*,
        const char* const*, const int*, const int*, int) = nullptr;
    int (__cdecl* result_status)(const PGresult*) = nullptr;
    char* (__cdecl* result_error_message)(const PGresult*) = nullptr;
    int (__cdecl* tuples)(const PGresult*) = nullptr;
    char* (__cdecl* value)(const PGresult*, int, int) = nullptr;
    void (__cdecl* clear)(PGresult*) = nullptr;

    PostgresApi()
    {
        library = LoadLibraryA("libpq.dll");
        if (library == nullptr) {
            throw std::runtime_error(
                "Could not load libpq.dll; add PostgreSQL's bin directory "
                "to PATH.");
        }
        try {
            connectdb = load<decltype(connectdb)>("PQconnectdb");
            status = load<decltype(status)>("PQstatus");
            error_message = load<decltype(error_message)>("PQerrorMessage");
            finish = load<decltype(finish)>("PQfinish");
            exec_params = load<decltype(exec_params)>("PQexecParams");
            result_status = load<decltype(result_status)>("PQresultStatus");
            result_error_message =
                load<decltype(result_error_message)>("PQresultErrorMessage");
            tuples = load<decltype(tuples)>("PQntuples");
            value = load<decltype(value)>("PQgetvalue");
            clear = load<decltype(clear)>("PQclear");
        } catch (...) {
            FreeLibrary(library);
            library = nullptr;
            throw;
        }
    }

    ~PostgresApi()
    {
        if (library != nullptr) FreeLibrary(library);
    }

private:
    template <typename Function>
    Function load(const char* name)
    {
        const auto address = GetProcAddress(library, name);
        if (address == nullptr) {
            throw std::runtime_error(
                std::string("PostgreSQL client is missing symbol ") + name);
        }
        static_assert(sizeof(Function) == sizeof(address),
                      "Windows API function pointers must have equal size.");
        Function function = nullptr;
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }
};

PostgresApi& postgres_api()
{
    static PostgresApi api;
    return api;
}

PGconn* pg_connectdb(const char* value)
{
    return postgres_api().connectdb(value);
}
int pg_status(const PGconn* value)
{
    return postgres_api().status(value);
}
char* pg_error_message(const PGconn* value)
{
    return postgres_api().error_message(value);
}
void pg_finish(PGconn* value)
{
    postgres_api().finish(value);
}
PGresult* pg_exec_params(PGconn* connection, const char* command,
                         const std::vector<std::string>& values)
{
    std::vector<const char*> parameters;
    parameters.reserve(values.size());
    for (const auto& value : values) parameters.push_back(value.c_str());
    return postgres_api().exec_params(
        connection, command, static_cast<int>(parameters.size()), nullptr,
        parameters.empty() ? nullptr : parameters.data(), nullptr, nullptr, 0);
}
int pg_result_status(const PGresult* value)
{
    return postgres_api().result_status(value);
}
char* pg_result_error_message(const PGresult* value)
{
    return postgres_api().result_error_message(value);
}
int pg_tuples(const PGresult* value)
{
    return postgres_api().tuples(value);
}
char* pg_value(const PGresult* value, int row, int column)
{
    return postgres_api().value(value, row, column);
}
void pg_clear(PGresult* value)
{
    postgres_api().clear(value);
}
#else
PGconn* pg_connectdb(const char* value) { return PQconnectdb(value); }
int pg_status(const PGconn* value) { return PQstatus(value); }
char* pg_error_message(const PGconn* value) { return PQerrorMessage(value); }
void pg_finish(PGconn* value) { PQfinish(value); }
PGresult* pg_exec_params(PGconn* connection, const char* command,
                         const std::vector<std::string>& values)
{
    std::vector<const char*> parameters;
    parameters.reserve(values.size());
    for (const auto& value : values) parameters.push_back(value.c_str());
    return PQexecParams(connection, command,
                        static_cast<int>(parameters.size()), nullptr,
                        parameters.empty() ? nullptr : parameters.data(),
                        nullptr, nullptr, 0);
}
int pg_result_status(const PGresult* value) { return PQresultStatus(value); }
char* pg_result_error_message(const PGresult* value)
{
    return PQresultErrorMessage(value);
}
int pg_tuples(const PGresult* value) { return PQntuples(value); }
char* pg_value(const PGresult* value, int row, int column)
{
    return PQgetvalue(value, row, column);
}
void pg_clear(PGresult* value) { PQclear(value); }
#endif

class PgResult {
public:
    explicit PgResult(PGresult* value) : value_(value) {}
    ~PgResult()
    {
        if (value_ != nullptr) pg_clear(value_);
    }
    PgResult(const PgResult&) = delete;
    PgResult& operator=(const PgResult&) = delete;
    PGresult* get() const { return value_; }

private:
    PGresult* value_;
};

PGresult* execute(PGconn* connection,
                  const char* sql,
                  const std::vector<std::string>& values)
{
    return pg_exec_params(connection, sql, values);
}

void require_pg_status(PGconn* connection,
                       PGresult* result,
                       int expected,
                       const char* operation)
{
    if (result == nullptr || pg_result_status(result) != expected) {
        const std::string details = result == nullptr
            ? pg_error_message(connection)
            : pg_result_error_message(result);
        if (result != nullptr) pg_clear(result);
        throw std::runtime_error(std::string(operation) + ": " + details);
    }
}

void execute_command(PGconn* connection, const char* sql,
                     const char* operation)
{
    PGresult* raw = execute(connection, sql, {});
    require_pg_status(connection, raw, PGRES_COMMAND_OK, operation);
    PgResult result(raw);
}

bool parse_money(const std::string& value, std::int64_t& cents)
{
    if (value.empty()) return false;
    const auto decimal = value.find('.');
    const std::string whole = value.substr(0, decimal);
    const std::string fractional = decimal == std::string::npos
        ? std::string() : value.substr(decimal + 1);
    if (whole.empty() || fractional.size() > 2 ||
        (decimal != std::string::npos && fractional.empty())) {
        return false;
    }
    if (!std::all_of(whole.begin(), whole.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        }) ||
        !std::all_of(fractional.begin(), fractional.end(),
                     [](unsigned char ch) {
                         return ch >= '0' && ch <= '9';
                     })) {
        return false;
    }
    try {
        const std::int64_t dollars = std::stoll(whole);
        const int cents_part = fractional.empty()
            ? 0
            : std::stoi(fractional + (fractional.size() == 1 ? "0" : ""));
        if (dollars < 0 ||
            dollars > (MAX_AMOUNT_CENTS - cents_part) / 100) {
            return false;
        }
        cents = dollars * 100 + cents_part;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

bool is_blank(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    });
}

std::string json_string(const std::string& value)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string escaped = "\"";
    for (const unsigned char character : value) {
        switch (character) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character < 0x20) {
                escaped += "\\u00";
                escaped += hex[character >> 4];
                escaped += hex[character & 0x0f];
            } else {
                escaped += static_cast<char>(character);
            }
        }
    }
    escaped += '"';
    return escaped;
}

bool parse_tax_rate(const std::string& value, std::int64_t& basis_points)
{
    std::int64_t cents = 0;
    if (!parse_money(value, cents) || cents > 10000) return false;
    basis_points = cents;
    return true;
}

}  // namespace

struct PostgresBillingRepository::Connection {
    PGconn* value = nullptr;
};

PostgresBillingRepository::PostgresBillingRepository(
    const char* connection_info)
    : connection_(new Connection)
{
    connection_->value =
        pg_connectdb(connection_info == nullptr ? "" : connection_info);
    if (connection_->value == nullptr ||
        pg_status(connection_->value) != CONNECTION_OK) {
        const std::string message = connection_->value == nullptr
            ? "Could not allocate PostgreSQL connection."
            : pg_error_message(connection_->value);
        if (connection_->value != nullptr) pg_finish(connection_->value);
        delete connection_;
        connection_ = nullptr;
        throw std::runtime_error("PostgreSQL connection failed: " + message);
    }
}

PostgresBillingRepository::~PostgresBillingRepository()
{
    if (connection_ != nullptr) {
        pg_finish(connection_->value);
        delete connection_;
    }
}

CreateBillResult PostgresBillingRepository::persist_bill(
    long long visit_id,
    const std::vector<BillItem>& items,
    std::int64_t subtotal_cents,
    std::int64_t tax_rate_basis_points,
    std::int64_t tax_cents,
    std::int64_t total_cents)
{
    execute_command(connection_->value, "BEGIN",
                    "Could not begin bill transaction");
    try {
        const std::vector<std::string> bill_values{
            std::to_string(visit_id), format_money(subtotal_cents),
            format_money(tax_rate_basis_points),
            format_money(tax_cents), format_money(total_cents)};
        PGresult* bill_raw = execute(
            connection_->value,
            "INSERT INTO bills "
            "(visit_id, subtotal, tax_rate, tax, total, status) "
            "SELECT $1::bigint, $2::numeric, $3::numeric, $4::numeric, "
            "$5::numeric, 'Unpaid' "
            "WHERE EXISTS (SELECT 1 FROM visits WHERE visit_id = $1::bigint) "
            "ON CONFLICT (visit_id) DO NOTHING "
            "RETURNING bill_id::text",
            bill_values);
        require_pg_status(connection_->value, bill_raw, PGRES_TUPLES_OK,
                          "Could not create bill");
        PgResult bill_result(bill_raw);
        if (pg_tuples(bill_result.get()) == 0) {
            const std::vector<std::string> visit_values{
                std::to_string(visit_id)};
            PGresult* visit_raw = execute(
                connection_->value,
                "SELECT EXISTS (SELECT 1 FROM visits "
                "WHERE visit_id = $1::bigint)::text",
                visit_values);
            require_pg_status(connection_->value, visit_raw, PGRES_TUPLES_OK,
                              "Could not verify visit");
            PgResult visit_result(visit_raw);
            const bool visit_exists =
                std::string(pg_value(visit_result.get(), 0, 0)) == "true";
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back bill transaction");
            return {visit_exists ? CreateBillStatus::AlreadyExists
                                 : CreateBillStatus::VisitMissing, {}};
        }

        const long long bill_id =
            std::stoll(pg_value(bill_result.get(), 0, 0));
        for (const auto& item : items) {
            const std::vector<std::string> item_values{
                std::to_string(bill_id), item.description,
                format_money(item.amount_cents)};
            PGresult* item_raw = execute(
                connection_->value,
                "INSERT INTO bill_items (bill_id, item, amount) "
                "VALUES ($1::bigint, $2, $3::numeric)",
                item_values);
            require_pg_status(connection_->value, item_raw, PGRES_COMMAND_OK,
                              "Could not add bill item");
            PgResult item_result(item_raw);
        }
        execute_command(connection_->value, "COMMIT",
                        "Could not commit bill transaction");
        return {CreateBillStatus::Created,
                {bill_id, visit_id, items, subtotal_cents,
                 tax_rate_basis_points, tax_cents, total_cents, "Unpaid"}};
    } catch (...) {
        execute_command(connection_->value, "ROLLBACK",
                        "Could not roll back bill transaction");
        throw;
    }
}

RecordPaymentResult PostgresBillingRepository::persist_payment(
    long long bill_id,
    std::int64_t amount_cents,
    const std::string& mode)
{
    execute_command(connection_->value, "BEGIN",
                    "Could not begin payment transaction");
    try {
        const std::vector<std::string> bill_values{std::to_string(bill_id)};
        // The row lock makes concurrent payments on one bill wait their turn.
        PGresult* bill_raw = execute(
            connection_->value,
            "SELECT total::text, status FROM bills "
            "WHERE bill_id = $1::bigint FOR UPDATE",
            bill_values);
        require_pg_status(connection_->value, bill_raw, PGRES_TUPLES_OK,
                          "Could not load bill");
        PgResult bill_result(bill_raw);
        if (pg_tuples(bill_result.get()) == 0) {
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back payment transaction");
            return {PaymentStatus::BillMissing, {}, 0, 0, {}};
        }
        std::int64_t total_cents = 0;
        if (!parse_money(pg_value(bill_result.get(), 0, 0), total_cents)) {
            throw std::runtime_error("Stored bill total is invalid.");
        }
        const std::string bill_status = pg_value(bill_result.get(), 0, 1);

        PGresult* paid_raw = execute(
            connection_->value,
            "SELECT COALESCE(SUM(amount), 0)::numeric(14,2)::text "
            "FROM payments WHERE bill_id = $1::bigint",
            bill_values);
        require_pg_status(connection_->value, paid_raw, PGRES_TUPLES_OK,
                          "Could not total payments");
        PgResult paid_result(paid_raw);
        std::int64_t paid_cents = 0;
        if (!parse_money(pg_value(paid_result.get(), 0, 0), paid_cents)) {
            throw std::runtime_error("Stored payment total is invalid.");
        }

        const PaymentStatus check = check_payment(
            bill_status, total_cents, paid_cents, amount_cents);
        if (check != PaymentStatus::Recorded) {
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back payment transaction");
            return {check, {}, total_cents, paid_cents, bill_status};
        }

        const std::vector<std::string> payment_values{
            std::to_string(bill_id), format_money(amount_cents), mode};
        PGresult* payment_raw = execute(
            connection_->value,
            "WITH next_payment AS ("
            "  SELECT nextval(pg_get_serial_sequence("
            "    'payments', 'payment_id')) AS payment_id"
            ") INSERT INTO payments "
            "(payment_id, bill_id, amount, mode, receipt_no) "
            "SELECT payment_id, $1::bigint, $2::numeric, $3, "
            "'RCPT-' || lpad(payment_id::text, 8, '0') FROM next_payment "
            "RETURNING payment_id::text, receipt_no",
            payment_values);
        require_pg_status(connection_->value, payment_raw, PGRES_TUPLES_OK,
                          "Could not record payment");
        PgResult payment_result(payment_raw);

        paid_cents += amount_cents;
        const std::string new_status =
            bill_status_after_payment(total_cents, paid_cents);
        const std::vector<std::string> status_values{
            std::to_string(bill_id), new_status};
        PGresult* status_raw = execute(
            connection_->value,
            "UPDATE bills SET status = $2 WHERE bill_id = $1::bigint",
            status_values);
        require_pg_status(connection_->value, status_raw, PGRES_COMMAND_OK,
                          "Could not update bill status");
        PgResult status_result(status_raw);

        const long long payment_id =
            std::stoll(pg_value(payment_result.get(), 0, 0));
        const std::string receipt_no = pg_value(payment_result.get(), 0, 1);
        execute_command(connection_->value, "COMMIT",
                        "Could not commit payment transaction");
        return {PaymentStatus::Recorded,
                {payment_id, bill_id, amount_cents, mode, receipt_no},
                total_cents, paid_cents, new_status};
    } catch (...) {
        execute_command(connection_->value, "ROLLBACK",
                        "Could not roll back payment transaction");
        throw;
    }
}

std::optional<Receipt> PostgresBillingRepository::find_receipt(
    long long payment_id)
{
    const std::vector<std::string> values{std::to_string(payment_id)};
    PGresult* raw = execute(
        connection_->value,
        "SELECT p.receipt_no, p.payment_id::text, p.bill_id::text, "
        "p.amount::numeric(14,2)::text, p.mode, "
        "to_char(p.paid_at AT TIME ZONE 'Asia/Kolkata', "
        "        'DD-MM-YYYY HH24:MI'), "
        "b.total::numeric(14,2)::text, "
        "(SELECT COALESCE(SUM(q.amount), 0)::numeric(14,2)::text "
        " FROM payments q WHERE q.bill_id = p.bill_id "
        " AND q.payment_id <= p.payment_id) "
        "FROM payments p JOIN bills b ON b.bill_id = p.bill_id "
        "WHERE p.payment_id = $1::bigint",
        values);
    require_pg_status(connection_->value, raw, PGRES_TUPLES_OK,
                      "Could not load receipt");
    PgResult result(raw);
    if (pg_tuples(result.get()) == 0) return std::nullopt;

    const auto stored_money = [](const char* value) {
        std::int64_t cents = 0;
        if (!parse_money(value, cents)) {
            throw std::runtime_error("Stored receipt amount is invalid.");
        }
        return cents;
    };
    Receipt receipt;
    receipt.receipt_no = pg_value(result.get(), 0, 0);
    receipt.payment_id = std::stoll(pg_value(result.get(), 0, 1));
    receipt.bill_id = std::stoll(pg_value(result.get(), 0, 2));
    receipt.amount_cents = stored_money(pg_value(result.get(), 0, 3));
    receipt.mode = pg_value(result.get(), 0, 4);
    receipt.paid_at = pg_value(result.get(), 0, 5);
    receipt.total_cents = stored_money(pg_value(result.get(), 0, 6));
    const std::int64_t paid_through =
        stored_money(pg_value(result.get(), 0, 7));
    receipt.balance_cents = receipt.total_cents - paid_through;
    receipt.bill_status =
        bill_status_after_payment(receipt.total_cents, paid_through);
    return receipt;
}

std::vector<PatientBill> PostgresBillingRepository::bills_for_patient(
    const std::string& patient_id)
{
    const auto stored_money = [](const char* value) {
        std::int64_t cents = 0;
        if (!parse_money(value, cents)) {
            throw std::runtime_error("Stored bill amount is invalid.");
        }
        return cents;
    };
    PGresult* bills_raw = execute(
        connection_->value,
        "SELECT b.bill_id::text, b.visit_id::text, b.subtotal::text, "
        "b.tax_rate::text, b.tax::text, b.total::text, b.status, "
        "(SELECT COALESCE(SUM(p.amount), 0)::numeric(14,2)::text "
        " FROM payments p WHERE p.bill_id = b.bill_id) "
        "FROM bills b JOIN visits v ON v.visit_id = b.visit_id "
        "JOIN appointments a ON a.appointment_id = v.appointment_id "
        "WHERE a.patient_id = $1 "
        "ORDER BY b.created_at DESC, b.bill_id DESC",
        {patient_id});
    require_pg_status(connection_->value, bills_raw, PGRES_TUPLES_OK,
                      "Could not load patient bills");
    PgResult bills_result(bills_raw);

    std::vector<PatientBill> bills;
    bills.reserve(static_cast<std::size_t>(pg_tuples(bills_result.get())));
    for (int row = 0; row < pg_tuples(bills_result.get()); ++row) {
        PatientBill entry;
        entry.bill.id = std::stoll(pg_value(bills_result.get(), row, 0));
        entry.bill.visit_id = std::stoll(pg_value(bills_result.get(), row, 1));
        entry.bill.subtotal_cents =
            stored_money(pg_value(bills_result.get(), row, 2));
        entry.bill.tax_rate_basis_points =
            stored_money(pg_value(bills_result.get(), row, 3));
        entry.bill.tax_cents =
            stored_money(pg_value(bills_result.get(), row, 4));
        entry.bill.total_cents =
            stored_money(pg_value(bills_result.get(), row, 5));
        entry.bill.status = pg_value(bills_result.get(), row, 6);
        entry.paid_cents = stored_money(pg_value(bills_result.get(), row, 7));

        PGresult* items_raw = execute(
            connection_->value,
            "SELECT item, amount::text FROM bill_items "
            "WHERE bill_id = $1::bigint ORDER BY bill_item_id",
            {std::to_string(entry.bill.id)});
        require_pg_status(connection_->value, items_raw, PGRES_TUPLES_OK,
                          "Could not load bill items");
        PgResult items_result(items_raw);
        for (int item = 0; item < pg_tuples(items_result.get()); ++item) {
            entry.bill.items.push_back({
                pg_value(items_result.get(), item, 0),
                stored_money(pg_value(items_result.get(), item, 1))
            });
        }
        bills.push_back(std::move(entry));
    }
    return bills;
}

CreateBillResult create_itemized_bill(
    BillRepository& repository,
    long long visit_id,
    const std::vector<BillItemInput>& inputs,
    const std::string& tax_rate_percent)
{
    if (visit_id <= 0 || inputs.empty() || inputs.size() > 100) {
        return {CreateBillStatus::InvalidRequest, {}};
    }
    std::int64_t tax_basis_points = 0;
    if (!parse_tax_rate(tax_rate_percent, tax_basis_points)) {
        return {CreateBillStatus::TaxConfigurationInvalid, {}};
    }
    std::vector<BillItem> items;
    items.reserve(inputs.size());
    std::int64_t subtotal = 0;
    for (const auto& input : inputs) {
        std::int64_t amount = 0;
        if (is_blank(input.description) || input.description.size() > 200 ||
            !parse_money(input.amount, amount) ||
            amount > MAX_AMOUNT_CENTS - subtotal) {
            return {CreateBillStatus::InvalidRequest, {}};
        }
        subtotal += amount;
        items.push_back({input.description, amount});
    }
    const std::int64_t tax =
        (subtotal * tax_basis_points + 5000) / 10000;
    if (tax > MAX_AMOUNT_CENTS - subtotal) {
        return {CreateBillStatus::InvalidRequest, {}};
    }
    return repository.persist_bill(
        visit_id, items, subtotal, tax_basis_points, tax, subtotal + tax);
}

PaymentStatus check_payment(const std::string& bill_status,
                            std::int64_t total_cents,
                            std::int64_t paid_cents,
                            std::int64_t amount_cents)
{
    if (bill_status == "Cancelled") return PaymentStatus::BillCancelled;
    if (amount_cents > total_cents - paid_cents) {
        return PaymentStatus::Overpayment;
    }
    return PaymentStatus::Recorded;
}

std::string bill_status_after_payment(std::int64_t total_cents,
                                      std::int64_t paid_cents)
{
    if (paid_cents >= total_cents) return "Paid";
    return paid_cents > 0 ? "Partially Paid" : "Unpaid";
}

RecordPaymentResult record_payment(
    BillRepository& repository,
    long long bill_id,
    const std::string& amount,
    const std::string& mode)
{
    std::int64_t amount_cents = 0;
    if (bill_id <= 0 || !parse_money(amount, amount_cents) ||
        amount_cents == 0 ||
        (mode != "Cash" && mode != "Card" && mode != "UPI")) {
        return {PaymentStatus::InvalidRequest, {}, 0, 0, {}};
    }
    return repository.persist_payment(bill_id, amount_cents, mode);
}

std::string format_money(std::int64_t amount_cents)
{
    const auto dollars = amount_cents / 100;
    const auto cents = amount_cents % 100;
    return std::to_string(dollars) + "." +
        (cents < 10 ? "0" : "") + std::to_string(cents);
}

std::string bill_to_json(const Bill& bill)
{
    std::string json =
        "{\"billId\":" + std::to_string(bill.id) +
        ",\"visitId\":" + std::to_string(bill.visit_id) +
        ",\"items\":[";
    for (std::size_t index = 0; index < bill.items.size(); ++index) {
        if (index != 0) json += ',';
        json += "{\"description\":" + json_string(bill.items[index].description) +
            ",\"amount\":" + json_string(format_money(
                bill.items[index].amount_cents)) + "}";
    }
    json += "],\"subtotal\":" + json_string(format_money(bill.subtotal_cents)) +
        ",\"taxRate\":" +
        json_string(format_money(bill.tax_rate_basis_points)) +
        ",\"tax\":" + json_string(format_money(bill.tax_cents)) +
        ",\"total\":" + json_string(format_money(bill.total_cents)) +
        ",\"status\":" + json_string(bill.status) + "}";
    return json;
}

std::string patient_bills_to_json(const std::vector<PatientBill>& bills)
{
    std::string json = "{\"bills\":[";
    for (std::size_t index = 0; index < bills.size(); ++index) {
        if (index != 0) json += ',';
        const auto& entry = bills[index];
        json += "{\"bill\":" + bill_to_json(entry.bill) +
            ",\"paid\":" + json_string(format_money(entry.paid_cents)) +
            ",\"balance\":" + json_string(format_money(
                entry.bill.total_cents - entry.paid_cents)) + "}";
    }
    return json + "]}";
}

std::string payment_to_json(const RecordPaymentResult& result)
{
    return "{\"paymentId\":" + std::to_string(result.payment.id) +
        ",\"billId\":" + std::to_string(result.payment.bill_id) +
        ",\"amount\":" +
        json_string(format_money(result.payment.amount_cents)) +
        ",\"mode\":" + json_string(result.payment.mode) +
        ",\"receiptNo\":" + json_string(result.payment.receipt_no) +
        ",\"balance\":" + json_string(format_money(
            result.total_cents - result.paid_cents)) +
        ",\"billStatus\":" + json_string(result.bill_status) + "}";
}

namespace {
std::string html_escape(const std::string& value)
{
    std::string out;
    for (const char ch : value) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += ch;
        }
    }
    return out;
}
}  // namespace

std::string render_receipt_html(const Receipt& r)
{
    const std::string rupee = "&#8377;";
    return
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<title>Receipt " + html_escape(r.receipt_no) + "</title>"
        "<style>body{font-family:sans-serif;max-width:420px;margin:24px auto}"
        "table{width:100%;border-collapse:collapse}"
        "td{padding:6px 0;border-bottom:1px solid #ccc}"
        "td:last-child{text-align:right}"
        "@media print{button{display:none}}</style></head><body>"
        "<h2>Payment Receipt</h2><table>"
        "<tr><td>Receipt No</td><td>" + html_escape(r.receipt_no) + "</td></tr>"
        "<tr><td>Bill No</td><td>" + std::to_string(r.bill_id) + "</td></tr>"
        "<tr><td>Amount Paid</td><td>" + rupee +
        format_money(r.amount_cents) + "</td></tr>"
        "<tr><td>Payment Mode</td><td>" + html_escape(r.mode) + "</td></tr>"
        "<tr><td>Payment Date</td><td>" + html_escape(r.paid_at) + "</td></tr>"
        "<tr><td>Bill Total</td><td>" + rupee +
        format_money(r.total_cents) + "</td></tr>"
        "<tr><td>Balance After Payment</td><td>" + rupee +
        format_money(r.balance_cents) + "</td></tr></table>"
        "<p><button onclick=\"window.print()\">Print</button></p>"
        "</body></html>";
}

}  // namespace hms::billing