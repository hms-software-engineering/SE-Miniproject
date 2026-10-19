
#include "../src/api/api.h"
#include "../src/services/auth_service.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    // Existing API error serialization tests
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

    // Authentication tests
    hms::AuthService auth;

    const auto weak_password =
        auth.register_user("test_weak_user", "weak");

    assert(!weak_password.success);

    const auto registration =
        auth.register_user("test_auth_user", "StrongPass1!");

    assert(registration.success);
    assert(registration.user_id == "test_auth_user");

    const auto duplicate =
        auth.register_user("test_auth_user", "AnotherPass2!");

    assert(!duplicate.success);

    const auto correct_login =
        auth.authenticate_user("test_auth_user", "StrongPass1!");

    assert(correct_login.success);

    const auto wrong_login =
        auth.authenticate_user("test_auth_user", "WrongPass1!");

    assert(!wrong_login.success);
    assert(wrong_login.message == "Invalid username or password");

    const auto unknown_login =
        auth.authenticate_user("unknown_test_user", "StrongPass1!");

    assert(!unknown_login.success);
    assert(unknown_login.message == "Invalid username or password");

    std::cout << "All API and authentication tests passed.\n";
    return 0;
}
