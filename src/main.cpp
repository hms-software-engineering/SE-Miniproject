
#include "api/api.h"
#include "services/auth_service.h"
#include "services/rbac_service.h"
#include "patients/patient_service.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
using SOCKET = int;
constexpr int INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#define closesocket close
#define SD_BOTH SHUT_RDWR
#define WSAGetLastError() errno
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr unsigned short PORT = 8080;
constexpr std::size_t MAX_REQUEST_SIZE = 1024 * 1024;
constexpr std::size_t MAX_HEADER_SIZE = 16 * 1024;
SSL* active_tls = nullptr;

std::string status_text(int status)
{
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 411: return "Length Required";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default:  return "Error";
    }
}

void append_utf8(std::string& output, unsigned int codepoint)
{
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

class JsonObjectReader {
public:
    explicit JsonObjectReader(const std::string& input) : input_(input) {}

    bool parse(std::map<std::string, std::string>& fields)
    {
        skip_space();
        if (!consume('{')) return false;
        skip_space();
        if (consume('}')) {
            skip_space();
            return position_ == input_.size();
        }

        while (position_ < input_.size()) {
            std::string key;
            std::string value;
            if (!parse_string(key)) return false;
            skip_space();
            if (!consume(':')) return false;
            skip_space();
            if (!parse_string(value)) return false;
            if (!fields.emplace(std::move(key), std::move(value)).second) {
                return false;
            }
            skip_space();
            if (consume('}')) {
                skip_space();
                return position_ == input_.size();
            }
            if (!consume(',')) return false;
            skip_space();
        }
        return false;
    }

private:
    void skip_space()
    {
        while (position_ < input_.size() &&
               (input_[position_] == ' ' || input_[position_] == '\t' ||
                input_[position_] == '\r' || input_[position_] == '\n')) {
            ++position_;
        }
    }

    bool consume(char expected)
    {
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    bool parse_hex_quad(unsigned int& value)
    {
        if (input_.size() - position_ < 4) return false;
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = input_[position_++];
            value <<= 4;
            if (ch >= '0' && ch <= '9') {
                value |= static_cast<unsigned int>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value |= static_cast<unsigned int>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                value |= static_cast<unsigned int>(ch - 'A' + 10);
            }
            else return false;
        }
        return true;
    }

    bool parse_string(std::string& output)
    {
        if (!consume('"')) return false;
        while (position_ < input_.size()) {
            const unsigned char ch =
                static_cast<unsigned char>(input_[position_++]);
            if (ch == '"') return true;
            if (ch < 0x20) return false;
            if (ch != '\\') {
                output.push_back(static_cast<char>(ch));
                continue;
            }
            if (position_ >= input_.size()) return false;
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                unsigned int codepoint = 0;
                if (!parse_hex_quad(codepoint)) return false;
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (input_.size() - position_ < 6 ||
                        input_[position_] != '\\' ||
                        input_[position_ + 1] != 'u') {
                        return false;
                    }
                    position_ += 2;
                    unsigned int low = 0;
                    if (!parse_hex_quad(low) ||
                        low < 0xdc00 || low > 0xdfff) {
                        return false;
                    }
                    codepoint = 0x10000 +
                        ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    return false;
                }
                append_utf8(output, codepoint);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    const std::string& input_;
    std::size_t position_ = 0;
};

std::string json_string(const std::string& value)
{
    std::ostringstream output;
    output << '"';
    for (char raw_character : value) {
        const auto ch = static_cast<unsigned char>(raw_character);
        switch (ch) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (ch < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                output << "\\u00" << hex[ch >> 4] << hex[ch & 0x0f];
            } else {
                output << static_cast<char>(ch);
            }
        }
    }
    output << '"';
    return output.str();
}

std::string make_request_id()
{
    static unsigned long long counter = 0;
    return "req-" + std::to_string(++counter);
}

std::string current_date_ddmmyyyy()
{
    const std::time_t now = std::time(nullptr);
    std::tm local_time{};
    if (localtime_s(&local_time, &now) != 0) {
        throw std::runtime_error("Could not determine the current date.");
    }
    std::ostringstream date;
    date << (local_time.tm_mday < 10 ? "0" : "") << local_time.tm_mday
         << '-' << (local_time.tm_mon + 1 < 10 ? "0" : "")
         << local_time.tm_mon + 1 << '-' << local_time.tm_year + 1900;
    return date.str();
}

std::string to_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

bool send_all(SOCKET client, const std::string& data)
{
    std::size_t sent = 0;

    while (sent < data.size()) {
        const std::size_t remaining = data.size() - sent;
        const int chunk = static_cast<int>(
            std::min<std::size_t>(remaining, 64 * 1024));

        const int result = SSL_write(active_tls, data.data() + sent, chunk);

        if (result == SOCKET_ERROR || result == 0)
            return false;

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

void send_response(
    SOCKET client,
    int status,
    const std::string& body)
{
    std::ostringstream response;

    response << "HTTP/1.1 " << status << ' '
             << status_text(status) << "\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    if (!send_all(client, response.str())) {
        std::cerr << "Failed to send HTTP response: "
                  << WSAGetLastError() << '\n';
    }
}

void send_error(
    SOCKET client,
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& request_id)
{
    const hms::ApiError error{
        status, code, message, request_id, ""
    };

    send_response(client, status, hms::make_error_json(error));
}

bool parse_json_body(const std::string& body,
                     std::map<std::string, std::string>& fields)
{
    return JsonObjectReader(body).parse(fields);
}

bool decode_query_value(const std::string& input, std::string& output)
{
    const auto hex_value = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    output.clear();
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index] == '+') {
            output.push_back(' ');
        } else if (input[index] == '%') {
            if (input.size() - index < 3) return false;
            const int high = hex_value(input[index + 1]);
            const int low = hex_value(input[index + 2]);
            if (high < 0 || low < 0) return false;
            output.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        } else {
            output.push_back(input[index]);
        }
    }
    return true;
}

bool read_query_parameter(const std::string& query,
                          const std::string& parameter,
                          std::string& value)
{
    std::size_t start = 0;
    bool found = false;
    while (start <= query.size()) {
        const auto end = query.find('&', start);
        const auto item = query.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        const auto separator = item.find('=');
        std::string key;
        std::string decoded_value;
        if (!decode_query_value(item.substr(0, separator), key) ||
            !decode_query_value(
                separator == std::string::npos ? "" : item.substr(separator + 1),
                decoded_value)) {
            return false;
        }
        if (key == parameter) {
            if (found) return false;
            found = true;
            value = std::move(decoded_value);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return found;
}

bool parse_patient_registration(
    const std::map<std::string, std::string>& fields,
    hms::patients::Registration& registration)
{
    if (fields.size() != 5 ||
        fields.find("name") == fields.end() ||
        fields.find("dateOfBirth") == fields.end() ||
        fields.find("gender") == fields.end() ||
        fields.find("phone") == fields.end() ||
        fields.find("address") == fields.end()) {
        return false;
    }
    registration = {
        fields.at("name"), fields.at("dateOfBirth"), fields.at("gender"),
        fields.at("phone"), fields.at("address")
    };
    return true;
}

std::string json_patient(const hms::patients::Patient& patient)
{
    return "{\"patientId\":" + json_string(patient.patient_id) +
        ",\"name\":" + json_string(patient.demographics.name) +
        ",\"dateOfBirth\":" +
        json_string(patient.demographics.date_of_birth) +
        ",\"gender\":" + json_string(patient.demographics.gender) +
        ",\"phone\":" + json_string(patient.demographics.phone) +
        ",\"address\":" + json_string(patient.demographics.address) + "}";
}

std::string validation_error(
    const std::vector<hms::patients::ValidationError>& errors)
{
    if (errors.empty()) return "Patient data is invalid.";
    return errors.front().field + ": " + errors.front().message;
}

bool parse_bearer_token(const std::string& authorization,
                        std::string& token)
{
    constexpr char prefix[] = "Bearer ";
    if (authorization.compare(0, sizeof(prefix) - 1, prefix) != 0) {
        return false;
    }
    token = authorization.substr(sizeof(prefix) - 1);
    return !token.empty() &&
           token.find_first_of(" \t\r\n") == std::string::npos;
}

bool is_json_content_type(const std::string& content_type)
{
    constexpr char media_type[] = "application/json";
    constexpr std::size_t length = sizeof(media_type) - 1;
    return content_type.compare(0, length, media_type) == 0 &&
           (content_type.size() == length || content_type[length] == ';' ||
            content_type[length] == ' ' || content_type[length] == '\t');
}

bool require_principal(
    SOCKET client,
    const std::string& authorization,
    const std::string& request_id,
    hms::AuthService& auth_service,
    hms::SessionPrincipal& principal,
    std::string& token)
{
    if (!parse_bearer_token(authorization, token)) {
        send_error(client, 401, "AUTHENTICATION_REQUIRED",
                   "Authentication required.", request_id);
        return false;
    }
    const auto authenticated = auth_service.authenticate_token(token);
    if (!authenticated.has_value()) {
        send_error(client, 401, "AUTHENTICATION_REQUIRED",
                   "Authentication required.", request_id);
        return false;
    }
    principal = *authenticated;
    return true;
}

std::optional<hms::Permission> permission_for_request(
    const std::string& method,
    const std::string& path)
{
    if (method == "POST" && path == "/api/v1/patients") {
        return hms::Permission::RegisterPatient;
    }
    if (method == "GET" && path == "/api/v1/patients/search") {
        return hms::Permission::SearchPatients;
    }
    if ((method == "PUT" || method == "PATCH") &&
        path.rfind("/api/v1/patients/", 0) == 0) {
        return hms::Permission::UpdatePatientData;
    }
    if (method == "GET" &&
        path == "/api/v1/patients/me/history") {
        return hms::Permission::ViewOwnHistoryBills;
    }
    if (method == "GET" && path == "/api/v1/patients/me/bills") {
        return hms::Permission::ViewOwnHistoryBills;
    }
    if (method == "GET" &&
        path.rfind("/api/v1/patients/", 0) == 0 &&
        path.size() > std::string("/api/v1/patients/").size() &&
        path.find("/history",
                  std::string("/api/v1/patients/").size()) !=
            std::string::npos) {
        return hms::Permission::ViewPatientHistory;
    }
    if (method == "GET" &&
        path == "/api/v1/appointments/free-slots") {
        return hms::Permission::ViewFreeSlots;
    }
    if (method == "POST" && path == "/api/v1/appointments") {
        return hms::Permission::BookAppointment;
    }
    if ((method == "PUT" || method == "PATCH" || method == "DELETE") &&
        path.rfind("/api/v1/appointments/", 0) == 0) {
        return hms::Permission::RescheduleCancelAppointment;
    }
    if (method == "GET" &&
        path == "/api/v1/doctors/me/appointments") {
        return hms::Permission::ViewOwnSchedule;
    }
    if (method == "POST" && path == "/api/v1/visits") {
        return hms::Permission::RecordVisitPrescribe;
    }
    if (method == "POST" &&
        (path == "/api/v1/bills" || path == "/api/v1/payments" ||
         path == "/api/v1/receipts")) {
        return hms::Permission::BillingPaymentsReceipts;
    }
    if (method == "GET" &&
        (path == "/api/v1/reports" ||
         path.rfind("/api/v1/reports/", 0) == 0)) {
        return hms::Permission::GenerateReports;
    }
    if (method == "GET" && path == "/api/v1/audit-log") {
        return hms::Permission::ViewAuditLog;
    }
    return std::nullopt;
}

void handle_client(SOCKET client,
                   hms::AuthService& auth_service,
                   hms::patients::PatientService& patient_service)
{
    const std::string request_id = make_request_id();
    std::string raw;
    char buffer[4096];

    std::size_t header_end = std::string::npos;
    std::size_t content_length = 0;
    bool has_content_length = false;
    bool headers_parsed = false;
    std::string authorization;
    std::string content_type;
    bool has_authorization_header = false;
    bool has_content_type_header = false;
    bool duplicate_auth_header = false;
    bool duplicate_content_type = false;

    // Read headers and the declared body, subject to a size limit.
    while (raw.size() < MAX_REQUEST_SIZE) {
        const int received = SSL_read(active_tls, buffer, static_cast<int>(sizeof(buffer)));

        if (received == SOCKET_ERROR) {
            std::cerr << "recv() failed: "
                      << WSAGetLastError() << '\n';
            return;
        }

        if (received == 0)
            break;

        raw.append(buffer, static_cast<std::size_t>(received));

        header_end = raw.find("\r\n\r\n");

        if (header_end == std::string::npos) {
            if (raw.size() > MAX_HEADER_SIZE) {
                send_error(client, 431, "HEADERS_TOO_LARGE",
                           "Request headers are too large.", request_id);
                return;
            }
            continue;
        }

        if (header_end > MAX_HEADER_SIZE) {
            send_error(client, 431, "HEADERS_TOO_LARGE",
                       "Request headers are too large.", request_id);
            return;
        }

        if (headers_parsed) {
            const std::size_t body_start = header_end + 4;
            if (raw.size() >= body_start + content_length) {
                break;
            }
            continue;
        }

        std::istringstream headers(raw.substr(0, header_end));
        std::string line;
        std::getline(headers, line); // Request line

        bool invalid_length = false;
        bool transfer_encoding = false;

        while (std::getline(headers, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const auto colon = line.find(':');
            if (colon == std::string::npos) {
                if (!line.empty()) {
                    send_error(client, 400, "BAD_REQUEST",
                               "Malformed HTTP header.", request_id);
                    return;
                }
                continue;
            }

            const std::string name =
                to_lower(line.substr(0, colon));

            std::string value = line.substr(colon + 1);
            const auto first = value.find_first_not_of(" \t");

            value = first == std::string::npos
                ? "" : value.substr(first);

            if (name == "content-length") {
                if (has_content_length) {
                    invalid_length = true;
                    break;
                }

                try {
                    std::size_t parsed = 0;
                    const auto length = std::stoull(value, &parsed);

                    if (parsed != value.size() ||
                        length > MAX_REQUEST_SIZE) {
                        invalid_length = true;
                        break;
                    }

                    content_length = static_cast<std::size_t>(length);
                    has_content_length = true;
                } catch (...) {
                    invalid_length = true;
                    break;
                }
            }

            if (name == "transfer-encoding")
                transfer_encoding = true;

            if (name == "authorization") {
                if (has_authorization_header) {
                    duplicate_auth_header = true;
                }
                has_authorization_header = true;
                authorization = value;
            }
            if (name == "content-type") {
                if (has_content_type_header) {
                    duplicate_content_type = true;
                }
                has_content_type_header = true;
                content_type = to_lower(value);
            }
        }

        if (invalid_length) {
            send_error(client, 400, "BAD_REQUEST",
                       "Invalid or duplicate Content-Length.",
                       request_id);
            return;
        }

        if (transfer_encoding) {
            send_error(client, 400, "BAD_REQUEST",
                       "Transfer-Encoding is not supported.",
                       request_id);
            return;
        }
        if (duplicate_auth_header || duplicate_content_type) {
            send_error(client, 400, "BAD_REQUEST",
                       "Duplicate authentication or content-type header.",
                       request_id);
            return;
        }

        const std::size_t body_start = header_end + 4;

        if (content_length > MAX_REQUEST_SIZE - body_start) {
            send_error(client, 413, "PAYLOAD_TOO_LARGE",
                       "Request is too large.", request_id);
            return;
        }

        headers_parsed = true;
        if (raw.size() >= body_start + content_length)
            break;
    }

    if (header_end == std::string::npos) {
        send_error(client, 400, "BAD_REQUEST",
                   "Incomplete HTTP headers.", request_id);
        return;
    }

    std::istringstream request(raw.substr(0, header_end));
    std::string method, path, version;
    request >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty()) {
        send_error(client, 400, "BAD_REQUEST",
                   "Invalid HTTP request line.", request_id);
        return;
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        send_error(client, 400, "BAD_REQUEST",
                   "Unsupported HTTP version.", request_id);
        return;
    }

    std::string query_string;
    const auto query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        query_string = path.substr(query_pos + 1);
        path.resize(query_pos);
    }

    std::cout << "Request: " << method << ' ' << path << '\n';
    hms::log_request(method, path, request_id);

    if (path != hms::API_BASE_PATH &&
        path.rfind(std::string(hms::API_BASE_PATH) + "/", 0) != 0) {
        send_error(client, 404, "NOT_FOUND",
                   "The requested API path was not found.", request_id);
        return;
    }

    if (method == "GET" && path == "/api/v1/health") {
        send_response(client, 200, "{\"status\":\"ok\"}");
        return;
    }

    if ((path == "/api/v1/auth/register" ||
         path == "/api/v1/auth/login" ||
         path == "/api/v1/auth/logout" ||
         path == "/api/v1/auth/me") &&
        !((method == "POST" &&
           (path == "/api/v1/auth/register" ||
            path == "/api/v1/auth/login" ||
            path == "/api/v1/auth/logout")) ||
          (method == "GET" && path == "/api/v1/auth/me"))) {
        send_error(client, 405, "METHOD_NOT_ALLOWED",
                   "The HTTP method is not supported for this resource.",
                   request_id);
        return;
    }

    if (method != "GET" && method != "POST" && method != "PUT" &&
        method != "PATCH" && method != "DELETE") {
        send_error(client, 405, "METHOD_NOT_ALLOWED",
                   "The HTTP method is not supported.", request_id);
        return;
    }

    if (path == "/api/v1/auth/register" ||
        path == "/api/v1/auth/login") {
        if (!is_json_content_type(content_type)) {
            send_error(client, 415, "UNSUPPORTED_MEDIA_TYPE",
                       "Content-Type must be application/json.", request_id);
            return;
        }
        const std::size_t body_start = header_end + 4;
        if (!has_content_length ||
            raw.size() < body_start + content_length) {
            send_error(client, 411, "LENGTH_REQUIRED",
                       "A Content-Length and request body are required.",
                       request_id);
            return;
        }
        std::map<std::string, std::string> fields;
        if (!parse_json_body(
                raw.substr(body_start, content_length), fields) ||
            fields.size() != 2 ||
            fields.find("username") == fields.end() ||
            fields.find("password") == fields.end()) {
            send_error(client, 400, "INVALID_REQUEST",
                       "Provide string fields username and password.",
                       request_id);
            return;
        }

        const auto result = path == "/api/v1/auth/register"
            ? auth_service.register_patient(fields.at("username"),
                                            fields.at("password"))
            : auth_service.authenticate_user(fields.at("username"),
                                             fields.at("password"));
        if (path == "/api/v1/auth/register") {
            if (!result.success) {
                const int status =
                    result.message == "Username already exists." ? 409 : 400;
                send_error(client, status, "REGISTRATION_FAILED",
                           result.message, request_id);
                return;
            }
            send_response(client, 201,
                "{\"message\":\"User registered successfully\","
                "\"userId\":" + json_string(result.user_id) +
                ",\"role\":" + json_string(
                    std::string(hms::role_name(result.role))) + "}");
            return;
        }
        if (!result.success) {
            send_error(client, 401, "INVALID_CREDENTIALS",
                       "Invalid username or password.", request_id);
            return;
        }
        send_response(client, 200,
            "{\"message\":\"Login successful\",\"userId\":" +
            json_string(result.user_id) + ",\"role\":" +
            json_string(std::string(hms::role_name(result.role))) +
            ",\"token\":" + json_string(result.token) + "}");
        return;
    }

    if (path == "/api/v1/auth/logout" ||
        path == "/api/v1/auth/me") {
        hms::SessionPrincipal principal;
        std::string token;
        if (!require_principal(client, authorization, request_id,
                               auth_service, principal, token)) {
            return;
        }
        if (path == "/api/v1/auth/logout") {
            auth_service.logout(token);
            send_response(client, 200, "{\"message\":\"Logged out.\"}");
        } else {
            send_response(client, 200,
                "{\"userId\":" + json_string(principal.user_id) +
                ",\"role\":" + json_string(
                    std::string(hms::role_name(principal.role))) + "}");
        }
        return;
    }

    if (path == "/api/v1/users" && method == "POST") {
        hms::SessionPrincipal principal;
        std::string token;
        if (!require_principal(client, authorization, request_id,
                               auth_service, principal, token)) {
            return;
        }
        std::string authorization_error;
        if (!hms::authorize(principal.role, hms::Permission::ManageUsers,
                            authorization_error)) {
            send_error(client, 403, "ACCESS_DENIED", authorization_error,
                       request_id);
            return;
        }
        if (!is_json_content_type(content_type)) {
            send_error(client, 415, "UNSUPPORTED_MEDIA_TYPE",
                       "Content-Type must be application/json.", request_id);
            return;
        }
        const std::size_t body_start = header_end + 4;
        std::map<std::string, std::string> fields;
        hms::Role role;
        if (!has_content_length ||
            raw.size() < body_start + content_length ||
            !parse_json_body(raw.substr(body_start, content_length), fields) ||
            fields.size() != 3 ||
            fields.find("username") == fields.end() ||
            fields.find("password") == fields.end() ||
            fields.find("role") == fields.end() ||
            !hms::parse_role(fields.at("role"), role)) {
            send_error(client, 400, "INVALID_REQUEST",
                       "Provide string fields username, password, and a valid role.",
                       request_id);
            return;
        }
        const auto result = auth_service.create_user(
            fields.at("username"), fields.at("password"), role);
        if (!result.success) {
            const int status =
                result.message == "Username already exists." ? 409 : 400;
            send_error(client, status, "USER_CREATION_FAILED",
                       result.message, request_id);
            return;
        }
        send_response(client, 201,
            "{\"userId\":" + json_string(result.user_id) +
            ",\"role\":" + json_string(
                std::string(hms::role_name(result.role))) + "}");
        return;
    }

    const auto required_permission = permission_for_request(method, path);
    if (required_permission.has_value()) {
        hms::SessionPrincipal principal;
        std::string token;
        if (!require_principal(client, authorization, request_id,
                               auth_service, principal, token)) {
            return;
        }
        std::string authorization_error;
        if (!hms::authorize(principal.role, *required_permission,
                            authorization_error)) {
            send_error(client, 403, "ACCESS_DENIED", authorization_error,
                       request_id);
            return;
        }

        if (method == "GET" && path == "/api/v1/patients/search") {
            std::string query;
            if (!read_query_parameter(query_string, "query", query)) {
                send_error(client, 400, "INVALID_QUERY",
                           "Provide one URL-encoded query parameter.",
                           request_id);
                return;
            }
            const auto result = patient_service.search_patients(query);
            std::string body = "{\"matches\":[";
            for (std::size_t index = 0; index < result.matches.size(); ++index) {
                if (index != 0) body += ',';
                body += json_patient(result.matches[index]);
            }
            body += "],\"message\":" + json_string(result.message) + "}";
            send_response(client, 200, body);
            return;
        }

        if (method == "POST" && path == "/api/v1/patients") {
            if (!is_json_content_type(content_type)) {
                send_error(client, 415, "UNSUPPORTED_MEDIA_TYPE",
                           "Content-Type must be application/json.", request_id);
                return;
            }
            const std::size_t body_start = header_end + 4;
            std::map<std::string, std::string> fields;
            hms::patients::Registration registration;
            if (!has_content_length ||
                raw.size() < body_start + content_length ||
                !parse_json_body(raw.substr(body_start, content_length), fields) ||
                !parse_patient_registration(fields, registration)) {
                send_error(client, 400, "INVALID_REQUEST",
                           "Provide name, dateOfBirth, gender, phone, and address.",
                           request_id);
                return;
            }
            const auto result = patient_service.register_patient(
                registration, current_date_ddmmyyyy());
            if (!result.saved) {
                send_error(client, 400, "INVALID_PATIENT",
                           validation_error(result.errors), request_id);
                return;
            }
            send_response(client, 201,
                          "{\"patient\":" + json_patient(result.patient) + "}");
            return;
        }

        constexpr char patient_path[] = "/api/v1/patients/";
        if ((method == "PUT" || method == "PATCH") &&
            path.rfind(patient_path, 0) == 0) {
            const std::string patient_id =
                path.substr(sizeof(patient_path) - 1);
            if (patient_id.size() != 12 ||
                patient_id.compare(0, 4, "PID-") != 0 ||
                !std::all_of(patient_id.begin() + 4, patient_id.end(),
                    [](unsigned char digit) {
                        return digit >= '0' && digit <= '9';
                    })) {
                send_error(client, 404, "PATIENT_NOT_FOUND",
                           "Patient was not found.", request_id);
                return;
            }
            if (!is_json_content_type(content_type)) {
                send_error(client, 415, "UNSUPPORTED_MEDIA_TYPE",
                           "Content-Type must be application/json.", request_id);
                return;
            }
            const std::size_t body_start = header_end + 4;
            std::map<std::string, std::string> fields;
            hms::patients::Registration registration;
            if (!has_content_length ||
                raw.size() < body_start + content_length ||
                !parse_json_body(raw.substr(body_start, content_length), fields) ||
                !parse_patient_registration(fields, registration)) {
                send_error(client, 400, "INVALID_REQUEST",
                           "Provide name, dateOfBirth, gender, phone, and address.",
                           request_id);
                return;
            }
            const auto result = patient_service.update_patient(
                patient_id, registration, current_date_ddmmyyyy(),
                principal.user_id);
            if (!result.saved) {
                const bool not_found = !result.errors.empty() &&
                    result.errors.front().field == "patientId";
                send_error(client, not_found ? 404 : 400,
                           not_found ? "PATIENT_NOT_FOUND" : "INVALID_PATIENT",
                           validation_error(result.errors), request_id);
                return;
            }
            send_response(client, 200,
                          "{\"patient\":" + json_patient(result.patient) + "}");
            return;
        }

        send_error(client, 501, "NOT_IMPLEMENTED",
                   "This authorized operation is not implemented yet.",
                   request_id);
        return;
    }

    send_error(client, 404, "NOT_FOUND",
               "The requested resource was not found.", request_id);
}

} // namespace

int main()
{
    // Keep diagnostics visible in the console.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "Starting HMS server...\n";

    std::unique_ptr<hms::PostgresAuthRepository> auth_repository;
    std::unique_ptr<hms::patients::PostgresPatientRepository>
        patient_repository;
    std::unique_ptr<hms::patients::PatientService> patient_service;
    try {
        auth_repository = std::make_unique<hms::PostgresAuthRepository>();
        const char* data_key = std::getenv("HMS_PATIENT_DATA_KEY");
        const char* name_index_key =
            std::getenv("HMS_PATIENT_NAME_INDEX_KEY");
        const char* phone_index_key =
            std::getenv("HMS_PATIENT_PHONE_INDEX_KEY");
        if (data_key == nullptr || name_index_key == nullptr ||
            phone_index_key == nullptr) {
            throw std::runtime_error(
                "Set HMS_PATIENT_DATA_KEY, HMS_PATIENT_NAME_INDEX_KEY, and "
                "HMS_PATIENT_PHONE_INDEX_KEY before starting the server.");
        }
        if (std::string(data_key) == name_index_key ||
            std::string(data_key) == phone_index_key) {
            throw std::runtime_error(
                "Patient encryption and blind-index keys must be distinct.");
        }
        patient_repository =
            std::make_unique<hms::patients::PostgresPatientRepository>(
                data_key);
        patient_service =
            std::make_unique<hms::patients::PatientService>(
                *patient_repository, name_index_key, phone_index_key);
        hms::AuthService auth_service(*auth_repository);

        const char* bootstrap_username =
            std::getenv("HMS_BOOTSTRAP_ADMIN_USERNAME");
        const char* bootstrap_password =
            std::getenv("HMS_BOOTSTRAP_ADMIN_PASSWORD");
        if ((bootstrap_username == nullptr) !=
            (bootstrap_password == nullptr)) {
            std::cerr << "Set both HMS_BOOTSTRAP_ADMIN_USERNAME and "
                         "HMS_BOOTSTRAP_ADMIN_PASSWORD, or neither.\n";
            return 1;
        }
        if (bootstrap_username != nullptr &&
            *bootstrap_username != '\0' &&
            *bootstrap_password != '\0') {
            const auto result = auth_service.create_user(
                bootstrap_username, bootstrap_password,
                hms::Role::Administrator);
            if (result.success) {
                std::cout << "Bootstrap administrator created with userId="
                          << result.user_id << ".\n";
            } else if (result.message == "Username already exists.") {
                std::cout << "Bootstrap administrator was not created: "
                             "the configured username already exists.\n";
            } else {
                std::cerr << "Bootstrap administrator creation failed: "
                          << result.message << '\n';
                return 1;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Could not initialize authentication: "
                  << error.what() << '\n';
        return 1;
    }
    hms::AuthService auth_service(*auth_repository);

#ifdef _WIN32
    WSADATA wsa_data{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup != 0) {
        std::cerr << "WSAStartup failed: " << startup << '\n';
        return 1;
    }
#endif

    const char* cert_file = std::getenv("HMS_TLS_CERT_FILE");
    const char* key_file = std::getenv("HMS_TLS_KEY_FILE");
    if (!cert_file || !*cert_file || !key_file || !*key_file) {
        std::cerr << "Set HMS_TLS_CERT_FILE and HMS_TLS_KEY_FILE to a TLS certificate and private key.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    SSL_CTX* tls_context = SSL_CTX_new(TLS_server_method());
    if (!tls_context) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    SSL_CTX_set_min_proto_version(tls_context, TLS1_2_VERSION);
    SSL_CTX_set_options(tls_context, SSL_OP_NO_COMPRESSION);
    if (SSL_CTX_use_certificate_chain_file(tls_context, cert_file) != 1 ||
        SSL_CTX_use_PrivateKey_file(tls_context, key_file, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(tls_context) != 1) {
        std::cerr << "Failed to load TLS certificate/private key or they do not match.\n";
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(tls_context);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "socket() failed: "
                  << WSAGetLastError() << '\n';
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
        std::cerr << "inet_pton() failed.\n";
        closesocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: "
                  << WSAGetLastError() << '\n';
        closesocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed: "
                  << WSAGetLastError() << '\n';
        closesocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "HMS API listening at https://127.0.0.1:"
              << PORT << "/api/v1\n"
              << "Health endpoint: GET /api/v1/health\n"
              << "Stop with Ctrl+C or close this console.\n";

    // Keep this simple server synchronous. Closing the console terminates
    // the process; no unsafe signal handler is needed.
    for (;;) {
        std::cout << "Waiting for a client...\n";

        SOCKET client = accept(server, nullptr, nullptr);

        if (client == INVALID_SOCKET) {
            std::cerr << "accept() failed: "
                      << WSAGetLastError() << '\n';
            break;
        }

        std::cout << "Accepted client connection; starting TLS handshake.\n";
        SSL* tls = SSL_new(tls_context);
        if (!tls || SSL_set_fd(tls, static_cast<int>(client)) != 1 || SSL_accept(tls) != 1) {
            std::cerr << "TLS handshake failed; rejecting connection.\n";
            ERR_print_errors_fp(stderr);
            if (tls) SSL_free(tls);
            shutdown(client, SD_BOTH);
            closesocket(client);
            continue;
        }
        active_tls = tls;

        // Ensure the TLS session and socket close after each request.
        try {
            handle_client(client, auth_service, *patient_service);
        } catch (const std::exception& e) {
            std::cerr << "Request handler exception: "
                      << e.what() << '\n';

            try {
                send_error(client, 500, "INTERNAL_ERROR",
                           "Internal server error.", make_request_id());
            } catch (...) {
                // Do not allow an error response failure to escape.
            }
        } catch (...) {
            std::cerr << "Unknown request handler exception.\n";
        }

        SSL_shutdown(tls);
        SSL_free(tls);
        active_tls = nullptr;
        shutdown(client, SD_BOTH);
        closesocket(client);

        std::cout << "Client connection closed.\n";
    }

    closesocket(server);
    SSL_CTX_free(tls_context);
#ifdef _WIN32
        WSACleanup();
#endif

    std::cout << "HMS server stopped.\n";
    return 0;
}
