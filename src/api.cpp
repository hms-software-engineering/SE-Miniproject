#include "api/api.h"

#include <iostream>
#include <string>

namespace hms {
namespace {

std::string escape_json(const std::string& value) {
    std::string escaped;

    for (char ch : value) {
        switch (ch) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                escaped += ' ';
            } else {
                escaped += ch;
            }
        }
    }

    return escaped;
}

} // namespace

void log_request(const std::string& method,
                 const std::string& path,
                 const std::string& request_id) {
    std::cout << "[INFO] requestId=" << request_id
              << " component=API"
              << " method=" << method
              << " path=" << path << '\n';
}

std::string make_error_json(const ApiError& error) {
    std::string json =
        "{\"error\":{\"code\":\"" + escape_json(error.code) +
        "\",\"message\":\"" + escape_json(error.message) + "\"";

    if (!error.field.empty()) {
        json += ",\"field\":\"" + escape_json(error.field) + "\"";
    }

    json += ",\"requestId\":\"" + escape_json(error.request_id) + "\"}}";

    return json;
}

} // namespace hms