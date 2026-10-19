#include "../src/api/api.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    hms::ApiError error{
        404,
        "NOT_FOUND",
        "The requested resource was not found",
        "req-test-001",
        ""
    };

    const std::string json = hms::make_error_json(error);

    assert(json.find("\"code\":\"NOT_FOUND\"") != std::string::npos);
    assert(json.find("\"message\":\"The requested resource was not found\"") != std::string::npos);
    assert(json.find("\"requestId\":\"req-test-001\"") != std::string::npos);
    assert(json.find("\"field\"") == std::string::npos);

    hms::ApiError field_error{
        400,
        "VALIDATION_FAILED",
        "Invalid input",
        "req-test-002",
        "name"
    };

    const std::string field_json = hms::make_error_json(field_error);

    assert(field_json.find("\"field\":\"name\"") != std::string::npos);

    hms::ApiError escaped_error{
        400,
        "VALIDATION_FAILED",
        "A \"quoted\" message",
        "req-test-003",
        ""
    };

    const std::string escaped_json = hms::make_error_json(escaped_error);
    assert(escaped_json.find("A \\\"quoted\\\" message") != std::string::npos);

    std::cout << "All API utility tests passed.\n";
    return 0;
}