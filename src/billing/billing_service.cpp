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
    long long bill_id, std::int64_t amount_cents, const std::string& mode)
{
    execute_command(connection_->value, "BEGIN",
                    "Could not begin payment transaction");
    try {
        const std::vector<std::string> bill_values{std::to_string(bill_id)};
        PGresult* bill_raw = execute(
            connection_->value,
            "SELECT total::text, status FROM bills "
            "WHERE bill_id = $1::bigint FOR UPDATE",
            bill_values);
        require_pg_status(connection_->value, bill_raw, PGRES_TUPLES_OK,
                          "Could not load bill for payment");
        PgResult bill_result(bill_raw);
        if (pg_tuples(bill_result.get()) == 0) {
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back payment transaction");
            return {RecordPaymentStatus::BillMissing, {}};
        }

        std::int64_t total_cents = 0;
        if (!parse_money(pg_value(bill_result.get(), 0, 0), total_cents)) {
            throw std::runtime_error(
                "The stored bill total is not a valid monetary amount.");
        }
        const std::string bill_status =
            pg_value(bill_result.get(), 0, 1);
        if (bill_status == "Cancelled") {
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back payment transaction");
            return {RecordPaymentStatus::BillCancelled, {}};
        }
        if (bill_status == "Paid") {
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back payment transaction");
            return {RecordPaymentStatus::AlreadyPaid, {}};
        }

        PGresult* paid_raw = execute(
            connection_->value,
            "SELECT coalesce(sum(amount), 0)::numeric(12,2)::text "
            "FROM payments WHERE bill_id = $1::bigint",
            bill_values);
        require_pg_status(connection_->value, paid_raw, PGRES_TUPLES_OK,
                          "Could not calculate paid amount");
        PgResult paid_result(paid_raw);
        std::int64_t paid_cents = 0;
        if (!parse_money(pg_value(paid_result.get(), 0, 0), paid_cents)) {
            throw std::runtime_error(
                "The stored payments are not a valid monetary amount.");
        }
        if (paid_cents >= total_cents) {
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back payment transaction");
            return {RecordPaymentStatus::AlreadyPaid, {}};
        }
        const std::int64_t remaining_cents = total_cents - paid_cents;
        if (amount_cents > remaining_cents) {
            execute_command(connection_->value, "ROLLBACK",
                            "Could not roll back payment transaction");
            return {RecordPaymentStatus::Overpayment, {}};
        }

        const std::vector<std::string> payment_values{
            std::to_string(bill_id), format_money(amount_cents), mode};
        PGresult* payment_raw = execute(
            connection_->value,
            "WITH next_payment AS ("
            "  SELECT nextval(pg_get_serial_sequence("
            "    'payments', 'payment_id')::regclass) AS payment_id"
            ")"
            " INSERT INTO payments "
            "(payment_id, bill_id, amount, mode, receipt_no)"
            " SELECT payment_id, $1::bigint, $2::numeric, $3,"
            "   'HMS-' || lpad(payment_id::text, 12, '0')"
            " FROM next_payment"
            " RETURNING payment_id::text, receipt_no",
            payment_values);
        require_pg_status(connection_->value, payment_raw, PGRES_TUPLES_OK,
                          "Could not record payment");
        PgResult payment_result(payment_raw);
        const long long payment_id =
            std::stoll(pg_value(payment_result.get(), 0, 0));
        const std::string receipt_no =
            pg_value(payment_result.get(), 0, 1);
        const std::int64_t new_remaining = remaining_cents - amount_cents;
        const std::string new_status =
            new_remaining == 0 ? "Paid" : "Partially Paid";
        const std::vector<std::string> status_values{
            std::to_string(bill_id), new_status};
        PGresult* update_raw = execute(
            connection_->value,
            "UPDATE bills SET status = $2 WHERE bill_id = $1::bigint",
            status_values);
        require_pg_status(connection_->value, update_raw, PGRES_COMMAND_OK,
                          "Could not update bill payment status");
        PgResult update_result(update_raw);
        execute_command(connection_->value, "COMMIT",
                        "Could not commit payment transaction");
        return {RecordPaymentStatus::Recorded,
                {payment_id, bill_id, amount_cents, new_remaining, mode,
                 receipt_no, new_status}};
    } catch (...) {
        execute_command(connection_->value, "ROLLBACK",
                        "Could not roll back payment transaction");
        throw;
    }
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

RecordPaymentResult record_payment(
    PaymentRepository& repository,
    long long bill_id,
    const std::string& amount,
    const std::string& mode)
{
    std::int64_t amount_cents = 0;
    if (bill_id <= 0 ||
        (mode != "Cash" && mode != "Card" && mode != "UPI") ||
        !parse_money(amount, amount_cents) || amount_cents <= 0) {
        return {RecordPaymentStatus::InvalidRequest, {}};
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

std::string payment_to_json(const Payment& payment)
{
    return "{\"paymentId\":" + std::to_string(payment.id) +
        ",\"billId\":" + std::to_string(payment.bill_id) +
        ",\"amount\":" + json_string(format_money(payment.amount_cents)) +
        ",\"mode\":" + json_string(payment.mode) +
        ",\"receiptNo\":" + json_string(payment.receipt_no) +
        ",\"remainingBalance\":" +
        json_string(format_money(payment.remaining_cents)) +
        ",\"billStatus\":" + json_string(payment.bill_status) + "}";
}

}  // namespace hms::billing
