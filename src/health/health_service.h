#pragma once

#include <string>

namespace hms::health {

struct Response {
    int http_status;
    std::string body;
};

Response liveness_response();
Response readiness_response(bool postgres_available);

} // namespace hms::health
