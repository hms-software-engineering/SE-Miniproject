#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace hms {

inline constexpr const char* API_BASE_PATH = "/api/v1";

struct ApiError {
    int status;
    std::string code;
    std::string message;
    std::string request_id;
    std::string field;
};

void log_request(const std::string& method,
                 const std::string& path,
                 const std::string& request_id);

std::string make_error_json(const ApiError& error);
std::optional<std::string> parse_bearer_token(
    std::string_view authorization);

} // namespace hms
