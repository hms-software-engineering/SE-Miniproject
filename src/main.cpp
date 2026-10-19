
#include "api/api.h"
#include "services/auth_service.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "Ws2_32.lib")

namespace {

using json = nlohmann::json;
constexpr unsigned short PORT = 8080;
hms::AuthService auth_service;

std::string make_request_id() {
    return "req-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
}

std::string status_text(int status) {
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 411: return "Length Required";
    case 413: return "Content Too Large";
    case 500: return "Internal Server Error";
    default:  return "Error";
    }
}

bool send_all(SOCKET client, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        int result = send(client, data.data() + sent,
                          static_cast<int>(data.size() - sent), 0);
        if (result == SOCKET_ERROR || result == 0) return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

void send_response(SOCKET client, int status, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << status_text(status) << "\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    send_all(client, response.str());
}

void send_error(SOCKET client, int status, const std::string& code,
                const std::string& message, const std::string& request_id) {
    hms::ApiError error{status, code, message, request_id, ""};
    send_response(client, status, hms::make_error_json(error));
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void handle_client(SOCKET client) {
    constexpr std::size_t MAX_REQUEST_SIZE = 16384;
    std::string raw;
    char buffer[4096];
    std::size_t header_end = std::string::npos;
    std::size_t content_length = 0;

    // Read the complete headers before deciding how much body to read.
    while (header_end == std::string::npos) {
        int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            closesocket(client);
            return;
        }
        raw.append(buffer, static_cast<std::size_t>(n));
        if (raw.size() > MAX_REQUEST_SIZE) {
            send_error(client, 413, "REQUEST_TOO_LARGE",
                       "The request is too large.", make_request_id());
            closesocket(client);
            return;
        }
        header_end = raw.find("\r\n\r\n");
    }

    std::istringstream request_line(raw.substr(0, raw.find("\r\n")));
    std::string method, path, version;
    request_line >> method >> path >> version;
    const std::string request_id = make_request_id();

    if (method.empty() || path.empty() || version.empty()) {
        send_error(client, 400, "BAD_REQUEST",
                   "The HTTP request is invalid.", request_id);
        closesocket(client);
        return;
    }

    // Read headers and extract Content-Length.
    std::size_t line_start = raw.find("\r\n") + 2;
    while (line_start < header_end) {
        std::size_t line_end = raw.find("\r\n", line_start);
        if (line_end == std::string::npos || line_end > header_end) break;
        std::string line = raw.substr(line_start, line_end - line_start);
        auto colon = line.find(':');
        if (colon != std::string::npos &&
            lowercase(line.substr(0, colon)) == "content-length") {
            try {
                content_length = std::stoul(line.substr(colon + 1));
            } catch (...) {
                send_error(client, 400, "BAD_REQUEST",
                           "Invalid Content-Length header.", request_id);
                closesocket(client);
                return;
            }
        }
        line_start = line_end + 2;
    }

    if (content_length > MAX_REQUEST_SIZE ||
        header_end + 4 + content_length > MAX_REQUEST_SIZE) {
        send_error(client, 413, "REQUEST_TOO_LARGE",
                   "The request is too large.", request_id);
        closesocket(client);
        return;
    }

    if ((method == "POST") && content_length == 0) {
        send_error(client, 411, "LENGTH_REQUIRED",
                   "A Content-Length header and request body are required.",
                   request_id);
        closesocket(client);
        return;
    }

    const std::size_t body_start = header_end + 4;
    while (raw.size() < body_start + content_length) {
        int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            send_error(client, 400, "BAD_REQUEST",
                       "The request body was incomplete.", request_id);
            closesocket(client);
            return;
        }
        raw.append(buffer, static_cast<std::size_t>(n));
        if (raw.size() > MAX_REQUEST_SIZE) {
            send_error(client, 413, "REQUEST_TOO_LARGE",
                       "The request is too large.", request_id);
            closesocket(client);
            return;
        }
    }

    hms::log_request(method, path, request_id);

    if (path.rfind(hms::API_BASE_PATH, 0) != 0) {
        send_error(client, 404, "NOT_FOUND",
                   "The requested API path was not found.", request_id);
    } else if (method == "GET" && path == "/api/v1/health") {
        send_response(client, 200, R"({"status":"ok"})");
    } else if (method != "POST") {
        send_error(client, 405, "METHOD_NOT_ALLOWED",
                   "This endpoint does not support that HTTP method.",
                   request_id);
    } else if (path == "/api/v1/auth/register" ||
               path == "/api/v1/auth/login") {
        try {
            const std::string body =
                raw.substr(body_start, content_length);
            const json input = json::parse(body);

            if (!input.is_object() ||
                !input.contains("username") ||
                !input.contains("password") ||
                !input["username"].is_string() ||
                !input["password"].is_string()) {
                send_error(client, 400, "INVALID_REQUEST",
                           "Provide string fields username and password.",
                           request_id);
            } else {
                const std::string username =
                    input["username"].get<std::string>();
                const std::string password =
                    input["password"].get<std::string>();

                if (username.empty() || username.size() > 100 ||
                    password.size() > 1024) {
                    send_error(client, 400, "INVALID_REQUEST",
                               "The supplied credentials are invalid.",
                               request_id);
                } else if (path == "/api/v1/auth/register") {
                    const auto result =
                        auth_service.register_user(username, password);
                    if (result.success) {
                        send_response(client, 201,
                            json{{"message", "User registered successfully"},
                                 {"user_id", result.user_id}}.dump());
                    } else {
                        send_error(client, 400, "REGISTRATION_FAILED",
                                   result.message, request_id);
                    }
                } else {
                    const auto result =
                        auth_service.authenticate_user(username, password);
                    if (result.success) {
                        send_response(client, 200,
                            json{{"message", "Login successful"},
                                 {"user_id", result.user_id}}.dump());
                    } else {
                        send_error(client, 401, "INVALID_CREDENTIALS",
                                   "Invalid username or password", request_id);
                    }
                }
            }
        } catch (const json::exception&) {
            send_error(client, 400, "INVALID_JSON",
                       "The request body must contain valid JSON.", request_id);
        } catch (...) {
            send_error(client, 500, "INTERNAL_ERROR",
                       "An unexpected error occurred.", request_id);
        }
    } else {
        send_error(client, 404, "NOT_FOUND",
                   "The requested resource was not found.", request_id);
    }

    closesocket(client);
}

} // namespace

int main() {
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "Failed to initialize Windows networking.\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "Failed to create server socket.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Could not bind to port " << PORT
                  << ". Is another server using it?\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen for connections.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "HMS API listening at http://127.0.0.1:"
              << PORT << "/api/v1\n"
              << "Health endpoint: http://127.0.0.1:"
              << PORT << "/api/v1/health\n"
              << "Auth endpoints: POST /api/v1/auth/register and "
                 "POST /api/v1/auth/login\n"
              << "Press Ctrl+C to stop.\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            std::cerr << "Failed to accept a connection.\n";
            continue;
        }
        handle_client(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
