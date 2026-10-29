#include "api/audit_log_handler.h"

#include "api/api.h"
#include "services/rbac_service.h"

#include <cctype>
#include <exception>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

namespace hms {
namespace {

AuditLogResponse error_response(int status, const char* code,
                                const char* message,
                                const std::string& request_id)
{
    return {status, make_error_json({status, code, message, request_id, ""})};
}

bool decode_component(std::string_view input, std::string& output)
{
    output.clear();
    for (std::size_t i = 0; i < input.size(); ++i) {
        unsigned char value = static_cast<unsigned char>(input[i]);
        if (value == '+') {
            value = ' ';
        } else if (value == '%') {
            if (input.size() - i < 3) return false;
            const auto hex = [](char c) {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int high = hex(input[i + 1]);
            const int low = hex(input[i + 2]);
            if (high < 0 || low < 0) return false;
            value = static_cast<unsigned char>((high << 4) | low);
            i += 2;
        }
        if (value < 0x20 || value == 0x7f) return false;
        output.push_back(static_cast<char>(value));
    }
    return true;
}

bool parse_filters(const std::string& query,
                   std::optional<AuditAction>& action,
                   std::optional<AuditRecordId>& record_id)
{
    if (query.size() > 512) return false;
    std::size_t offset = 0;
    while (offset < query.size()) {
        const std::size_t end = query.find('&', offset);
        const std::string_view item(query.data() + offset,
            (end == std::string::npos ? query.size() : end) - offset);
        if (item.empty()) return false;
        const std::size_t equal = item.find('=');
        if (equal == std::string_view::npos) return false;
        std::string key;
        std::string value;
        if (!decode_component(item.substr(0, equal), key) ||
            !decode_component(item.substr(equal + 1), value) || value.empty())
            return false;
        if (key == "action") {
            if (action) return false;
            action = parse_audit_action(value);
            if (!action) return false;
        } else if (key == "recordId") {
            if (record_id) return false;
            record_id = AuditRecordId::parse(value);
            if (!record_id) return false;
        } else {
            return false;
        }
        if (end == std::string::npos) break;
        offset = end + 1;
        if (offset == query.size()) return false;
    }
    return true;
}

nlohmann::json serialize_records(const std::vector<AuditRecord>& records)
{
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& record : records) {
        entries.push_back({
            {"userId", record.user_id ? nlohmann::json(*record.user_id)
                                      : nlohmann::json(nullptr)},
            {"role", audit_role_name(record.role)},
            {"action", audit_action_name(record.action)},
            {"recordId", record.record_id
                ? nlohmann::json(record.record_id->value())
                : nlohmann::json(nullptr)},
            {"timestamp", record.timestamp},
            {"sourceIp", record.source_ip}
        });
    }
    return {{"entries", std::move(entries)}};
}

} // namespace

AuditLogResponse handle_audit_log(const std::string& method,
                                  const std::string& path,
                                  const std::string& query,
                                  const std::string& authorization,
                                  const std::string& source_ip,
                                  const std::string& request_id,
                                  AuthService& auth_service)
{
    if (path != "/api/v1/audit-log")
        return error_response(404, "NOT_FOUND",
                              "The requested resource was not found.", request_id);
    if (method != "GET")
        return error_response(405, "METHOD_NOT_ALLOWED",
                              "The HTTP method is not supported.", request_id);

    const auto token = parse_bearer_token(authorization);
    if (!token)
        return error_response(401, "AUTHENTICATION_REQUIRED",
                              "Authentication required.", request_id);
    try {
        const auto principal = auth_service.authenticate_token(*token);
        if (!principal)
            return error_response(401, "AUTHENTICATION_REQUIRED",
                                  "Authentication required.", request_id);
        std::string authorization_error;
        if (!authorize(principal->role, Permission::ViewAuditLog,
                       authorization_error))
            return error_response(403, "ACCESS_DENIED", "Access denied.",
                                  request_id);

        std::optional<AuditAction> action;
        std::optional<AuditRecordId> record_id;
        if (!parse_filters(query, action, record_id))
            return error_response(400, "INVALID_AUDIT_FILTER",
                                  "Audit filters are invalid.", request_id);
        const auto read_result = auth_service.read_audit_events(
            *token, action, record_id);
        if (read_result.status == AuditReadStatus::AuthenticationRequired)
            return error_response(401, "AUTHENTICATION_REQUIRED",
                                  "Authentication required.", request_id);
        if (read_result.status == AuditReadStatus::AccessDenied)
            return error_response(403, "ACCESS_DENIED", "Access denied.", request_id);
        auth_service.record_audit_event(AuditEvent{
            principal->user_id,
            audit_role_name(audit_role(principal->role)),
            audit_action_name(AuditAction::ViewAuditLog), "", source_ip});
        return {200, serialize_records(read_result.records).dump()};
    } catch (const std::exception&) {
        return error_response(500, "INTERNAL_ERROR",
                              "The request could not be completed.", request_id);
    }
}

} // namespace hms
