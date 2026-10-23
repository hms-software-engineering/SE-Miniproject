
#include "api/api.h"
#include "services/auth_service.h"
#include <exception>
#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "appointments/appointments.h"
#include "appointments/postgres_appointment_repository.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <string>


namespace {

using json = nlohmann::json;

constexpr int PORT = 8080;
constexpr std::size_t MAX_REQUEST_SIZE = 16384;

hms::PostgresAuthRepository auth_repo;
hms::AuthService auth_service(auth_repo);
hms::PostgresAppointmentRepository appointment_repo;

std::string make_request_id() {
    return "req-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    return value;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");

    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

void send_response(httplib::Response& response,
                   int status,
                   const std::string& body) {
    response.status = status;
    response.set_content(body, "application/json; charset=utf-8");
}

void send_error(httplib::Response& response,
                int status,
                const std::string& code,
                const std::string& message,
                const std::string& request_id) {
    hms::ApiError error{status, code, message, request_id, ""};
    send_response(response, status, hms::make_error_json(error));
}

bool get_bearer_token(const httplib::Request& request,
                      std::string& token) {
    if (!request.has_header("Authorization")) {
        return false;
    }

    const std::string authorization =
        request.get_header_value("Authorization");

    if (authorization.size() < 7 ||
        lowercase(authorization.substr(0, 7)) != "bearer ") {
        return false;
    }

    token = trim(authorization.substr(7));
    return !token.empty();
}

bool parse_credentials(const httplib::Request& request,
                       httplib::Response& response,
                       const std::string& request_id,
                       std::string& username,
                       std::string& password) {
    if (request.body.size() > MAX_REQUEST_SIZE) {
        send_error(response, 413, "REQUEST_TOO_LARGE",
                   "The request is too large.", request_id);
        return false;
    }

    try {
        const json input = json::parse(request.body);

        if (!input.is_object() ||
            !input.contains("username") ||
            !input.contains("password") ||
            !input["username"].is_string() ||
            !input["password"].is_string()) {
            send_error(response, 400, "INVALID_REQUEST",
                       "Provide string fields username and password.",
                       request_id);
            return false;
        }

        username = input["username"].get<std::string>();
        password = input["password"].get<std::string>();

        if (username.empty() || username.size() > 100 ||
            password.size() > 1024) {
            send_error(response, 400, "INVALID_REQUEST",
                       "The supplied credentials are invalid.", request_id);
            return false;
        }

        return true;
    } catch (const json::exception&) {
        send_error(response, 400, "INVALID_JSON",
                   "The request body must contain valid JSON.", request_id);
        return false;
    }
}

void handle_register_or_login(const httplib::Request& request,
                              httplib::Response& response,
                              const std::string& request_id,
                              bool is_registration) {
    std::string username;
    std::string password;

    if (!parse_credentials(
            request, response, request_id, username, password)) {
        return;
    }

    if (is_registration) {
        const auto result =
            auth_service.register_patient(username, password);

        if (result.success) {
            send_response(response, 201,
                json{
                    {"message", "User registered successfully"},
                    {"user_id", result.user_id}
                }.dump());
        } else {
            send_error(response, 400, "REGISTRATION_FAILED",
                       result.message, request_id);
        }

        return;
    }

    const auto result =
        auth_service.authenticate_user(username, password);

    if (!result.success) {
        send_error(response, 401, "INVALID_CREDENTIALS",
                   "Invalid username or password", request_id);
        return;
    }


    send_response(response, 200,
    json{
        {"message", "Login successful"},
        {"user_id", result.user_id},
        {"role", std::string(hms::role_name(result.role))},
        {"token", result.token},
        {"token_type", "Bearer"}
    }.dump());
}

void handle_logout(const httplib::Request& request,
                   httplib::Response& response,
                   const std::string& request_id) {
    std::string token;

    if (!get_bearer_token(request, token)) {
        send_error(response, 401, "UNAUTHORIZED",
                   "A valid bearer token is required.", request_id);
        return;
    }

    if (auth_service.authenticate_token(token).has_value()) {
        auth_service.logout(token);
        send_response(response, 200,
                      R"({"message":"Logout successful"})");
    } else {
        send_error(response, 401, "INVALID_SESSION",
                   "The session is invalid or has expired.", request_id);
    }
}


void handle_protected_post(const httplib::Request& request,
                           httplib::Response& response,
                           const std::string& request_id) {
    std::string token;

    if (!get_bearer_token(request, token)) {
        send_error(response, 401, "UNAUTHORIZED",
                   "A valid bearer token is required.", request_id);
        return;
    }

    const auto session = auth_service.authenticate_token(token);

    if (!session.has_value()) {
        send_error(response, 401, "INVALID_SESSION",
                   "The session is invalid or has expired. Please log in again.",
                   request_id);
        return;
    }

    if (request.path != "/api/v1/appointments") {
        send_error(response, 404, "NOT_FOUND",
                   "The requested resource was not found.", request_id);
        return;
    }

    try {
        const auto input = json::parse(request.body);

        if (!input.is_object() ||
            !input.contains("patientId") ||
            !input.contains("doctorId") ||
            !input.contains("date") ||
            !input.contains("time") ||
            !input["patientId"].is_string() ||
            !input["doctorId"].is_number_integer() ||
            !input["date"].is_string() ||
            !input["time"].is_string()) {
            send_error(response, 400, "INVALID_REQUEST",
                       "Provide patientId, integer doctorId, date and time.",
                       request_id);
            return;
        }

        const std::string patient_id =
            input["patientId"].get<std::string>();
        const long long doctor_id =
            input["doctorId"].get<long long>();
        const std::string date =
            input["date"].get<std::string>();
        const std::string time =
            input["time"].get<std::string>();

        const auto result = hms::book_appointment(
            appointment_repo, patient_id, doctor_id, date, time);

        if (!result.ok) {
            response.status = result.http_status;
            response.set_content(
                hms::booking_error_to_json(result, request_id),
                "application/json; charset=utf-8");
            return;
        }

        send_response(response, result.http_status,
                      hms::appointment_to_json(result.appointment));
    } catch (const json::exception&) {
        send_error(response, 400, "INVALID_JSON",
                   "The request body must contain valid JSON.", request_id);
    }
}


void handle_request(const httplib::Request& request,
                    httplib::Response& response) {
    const std::string request_id = make_request_id();
    const std::string method = request.method;
    const std::string path = request.path;

    hms::log_request(method, path, request_id);

    if (request.body.size() > MAX_REQUEST_SIZE) {
        send_error(response, 413, "REQUEST_TOO_LARGE",
                   "The request is too large.", request_id);
        return;
    }

    if (path.rfind(hms::API_BASE_PATH, 0) != 0) {
        send_error(response, 404, "NOT_FOUND",
                   "The requested API path was not found.", request_id);
        return;
    }

    if (method == "GET" && path == "/api/v1/health") {
        send_response(response, 200, R"({"status":"ok"})");
        return;
    }

    if (path == "/api/v1/auth/register" ||
        path == "/api/v1/auth/login") {
        if (method != "POST") {
            send_error(response, 405, "METHOD_NOT_ALLOWED",
                       "This endpoint does not support that HTTP method.",
                       request_id);
            return;
        }

        handle_register_or_login(
            request, response, request_id,
            path == "/api/v1/auth/register");
        return;
    }

    if (path == "/api/v1/auth/logout") {
        if (method != "POST") {
            send_error(response, 405, "METHOD_NOT_ALLOWED",
                       "This endpoint does not support that HTTP method.",
                       request_id);
            return;
        }

        handle_logout(request, response, request_id);
        return;
    }

    if (method != "POST") {
        send_error(response, 405, "METHOD_NOT_ALLOWED",
                   "This endpoint does not support that HTTP method.",
                   request_id);
        return;
    }

    handle_protected_post(request, response, request_id);
}

void handle_method_not_allowed(const httplib::Request& request,
                               httplib::Response& response) {
    const std::string request_id = make_request_id();

    hms::log_request(request.method, request.path, request_id);
    send_error(response, 405, "METHOD_NOT_ALLOWED",
               "This endpoint does not support that HTTP method.",
               request_id);
}

} // namespace

int main() {
    httplib::Server server;

    server.set_payload_max_length(MAX_REQUEST_SIZE);

    server.set_exception_handler(
    [](const httplib::Request&, httplib::Response& response,
       std::exception_ptr ep) {
        try {
            if (ep) {
                std::rethrow_exception(ep);
            }
        } catch (const std::exception& e) {
            std::cerr << "Unhandled server exception: "
                      << e.what() << '\n';
        } catch (...) {
            std::cerr << "Unhandled unknown server exception\n";
        }

        hms::ApiError error{
            500, "INTERNAL_ERROR",
            "An unexpected error occurred.",
            "req-internal", ""
        };
        response.status = 500;
        response.set_content(
            hms::make_error_json(error),
            "application/json; charset=utf-8");
    });
    server.Get("/api/v1/health", handle_request);
    server.Post("/api/v1/auth/register", handle_request);
    server.Post("/api/v1/auth/login", handle_request);
    server.Post("/api/v1/auth/logout", handle_request);
    server.Post("/api/v1/appointments", handle_request);

    // Route other API paths through the same authorization/error logic.
    server.Post(R"(/api/v1/.*)", handle_request);

    // Keep errors in the API's JSON format for unregistered paths/methods.
    server.set_error_handler(
        [](const httplib::Request& request,
           httplib::Response& response) {
            const std::string request_id = make_request_id();
            hms::log_request(request.method, request.path, request_id);

            if (response.status == 404) {
                send_error(response, 404, "NOT_FOUND",
                           "The requested resource was not found.",
                           request_id);
            } else if (response.status == 405) {
                send_error(response, 405, "METHOD_NOT_ALLOWED",
                           "This endpoint does not support that HTTP method.",
                           request_id);
            }
        });

    std::cout << "HMS API listening at http://127.0.0.1:"
              << PORT << "/api/v1\n"
              << "Health endpoint: GET /api/v1/health\n"
              << "Auth endpoints: POST /api/v1/auth/register, "
                 "POST /api/v1/auth/login, POST /api/v1/auth/logout\n"
              << "Press Ctrl+C to stop.\n";

    if (!server.listen("127.0.0.1", PORT)) {
        std::cerr << "Could not start server on port " << PORT
                  << ". Is another server using it?\n";
        return 1;
    }

    return 0;
}
