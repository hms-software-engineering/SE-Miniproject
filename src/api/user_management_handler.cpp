#include "api/user_management_handler.h"

#include "api/api.h"
#include "services/rbac_service.h"

#include <charconv>
#include <cctype>
#include <exception>
#include <nlohmann/json.hpp>

namespace hms {
namespace {

UserManagementResponse error_response(
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& request_id)
{
    return {
        status,
        make_error_json(ApiError{status, code, message, request_id, ""})
    };
}

bool is_positive_user_id(const std::string& value, long long& id)
{
    if (value.empty()) {
        return false;
    }
    for (const unsigned char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), id);
    return result.ec == std::errc{} &&
           result.ptr == value.data() + value.size() && id > 0;
}

bool is_json_content_type(const std::string& content_type)
{
    constexpr char media_type[] = "application/json";
    constexpr std::size_t length = sizeof(media_type) - 1;
    bool matches = content_type.size() >= length;
    for (std::size_t index = 0; matches && index < length; ++index) {
        matches = std::tolower(static_cast<unsigned char>(content_type[index])) ==
                  media_type[index];
    }
    return matches &&
           (content_type.size() == length || content_type[length] == ';' ||
            content_type[length] == ' ' || content_type[length] == '\t');
}

} // namespace

UserManagementResponse handle_user_deactivation(
    const std::string& method,
    const std::string& path,
    const std::string& authorization,
    const std::string& content_type,
    const std::string& body,
    bool has_content_length,
    const std::string& source_ip,
    const std::string& request_id,
    AuthService& auth_service)
{
    if (method != "PATCH") {
        return error_response(405, "METHOD_NOT_ALLOWED",
                              "The HTTP method is not supported.", request_id);
    }

    const auto token = parse_bearer_token(authorization);
    if (!token.has_value()) {
        return error_response(401, "AUTHENTICATION_REQUIRED",
                              "Authentication required.", request_id);
    }
    const auto principal = auth_service.authenticate_token(*token);
    if (!principal.has_value()) {
        return error_response(401, "AUTHENTICATION_REQUIRED",
                              "Authentication required.", request_id);
    }
    std::string authorization_error;
    if (!authorize(principal->role, Permission::ManageUsers,
                   authorization_error)) {
        return error_response(403, "ACCESS_DENIED", authorization_error,
                              request_id);
    }

    constexpr char user_path[] = "/api/v1/users/";
    const std::string prefix(user_path);
    if (path.compare(0, prefix.size(), prefix) != 0) {
        return error_response(404, "NOT_FOUND",
                              "The requested resource was not found.",
                              request_id);
    }
    const std::string supplied_user_id = path.substr(prefix.size());
    long long numeric_user_id = 0;
    if (supplied_user_id.find('/') != std::string::npos ||
        !is_positive_user_id(supplied_user_id, numeric_user_id)) {
        return error_response(400, "INVALID_USER_ID",
                              "User ID must be a positive integer.",
                              request_id);
    }

    if (!is_json_content_type(content_type)) {
        return error_response(415, "UNSUPPORTED_MEDIA_TYPE",
                              "Content-Type must be application/json.",
                              request_id);
    }
    if (!has_content_length) {
        return error_response(400, "INVALID_REQUEST",
                              "A Content-Length and request body are required.",
                              request_id);
    }

    const auto request = nlohmann::json::parse(body, nullptr, false);
    if (request.is_discarded() || !request.is_object() ||
        request.size() != 1 || !request.contains("action") ||
        !request.at("action").is_string() ||
        request.at("action") != "deactivate") {
        return error_response(
            400, "INVALID_REQUEST",
            "Provide the string field action with value deactivate.",
            request_id);
    }

    const std::string user_id = std::to_string(numeric_user_id);
    DeactivateUserStatus result;
    try {
        result = auth_service.deactivate_user(
            *principal, user_id, source_ip);
    } catch (const std::exception&) {
        return error_response(500, "INTERNAL_ERROR",
                              "The request could not be completed.",
                              request_id);
    }
    switch (result) {
    case DeactivateUserStatus::Deactivated:
        return {
            200,
            "{\"userId\":\"" + user_id +
                "\",\"active\":false,\"message\":\"User deactivated.\"}"
        };
    case DeactivateUserStatus::NotFound:
        return error_response(404, "USER_NOT_FOUND",
                              "User was not found.", request_id);
    case DeactivateUserStatus::ActorNotAuthorized:
        return error_response(401, "AUTHENTICATION_REQUIRED",
                              "Authentication required.", request_id);
    case DeactivateUserStatus::SelfDeactivation:
        return error_response(409, "SELF_DEACTIVATION_NOT_ALLOWED",
                              "Administrators cannot deactivate their own account.",
                              request_id);
    case DeactivateUserStatus::LastAdministrator:
        return error_response(409, "LAST_ADMINISTRATOR",
                              "At least one active Administrator must remain.",
                              request_id);
    }

    return error_response(500, "INTERNAL_ERROR",
                          "The request could not be completed.", request_id);
}

} // namespace hms
