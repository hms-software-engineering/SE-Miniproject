#include "api/api.h"

#include <iostream>

int main() {
    const std::string request_id = "req-test-001";

    hms::log_request("GET", "/api/v1/health", request_id);

    hms::ApiError error{
        404,
        "NOT_FOUND",
        "The requested resource was not found",
        request_id,
        ""
    };

    std::cout << hms::make_error_json(error) << '\n';

    return 0;
}