
#include "api/api.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

namespace {

constexpr unsigned short PORT = 8080;

std::string make_request_id() {
    const auto now = std::chrono::steady_clock::now()
                         .time_since_epoch()
                         .count();
    return "req-" + std::to_string(now);
}

std::string status_text(int status) {
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default:  return "Error";
    }
}

bool send_all(SOCKET client, const std::string& data) {
    std::size_t sent = 0;

    while (sent < data.size()) {
        int result = send(
            client,
            data.data() + sent,
            static_cast<int>(data.size() - sent),
            0
        );

        if (result == SOCKET_ERROR || result == 0) {
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

void send_response(
    SOCKET client,
    int status,
    const std::string& body
) {
    std::ostringstream response;

    response << "HTTP/1.1 " << status << ' '
             << status_text(status) << "\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    send_all(client, response.str());
}

void send_error(
    SOCKET client,
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& request_id
) {
    hms::ApiError error{
        status,
        code,
        message,
        request_id,
        ""
    };

    send_response(client, status, hms::make_error_json(error));
}

void handle_client(SOCKET client) {
    char buffer[8192]{};

    int received = recv(
        client,
        buffer,
        static_cast<int>(sizeof(buffer) - 1),
        0
    );

    if (received <= 0) {
        closesocket(client);
        return;
    }

    std::istringstream request(
        std::string(buffer, static_cast<std::size_t>(received))
    );

    std::string method;
    std::string path;
    std::string version;

    request >> method >> path >> version;

    const std::string request_id = make_request_id();

    if (method.empty() || path.empty() || version.empty()) {
        hms::log_request("UNKNOWN", "UNKNOWN", request_id);
        send_error(
            client, 400, "BAD_REQUEST",
            "The HTTP request is invalid.", request_id
        );
        closesocket(client);
        return;
    }

    hms::log_request(method, path, request_id);

    if (path.rfind(hms::API_BASE_PATH, 0) != 0) {
        send_error(
            client, 404, "NOT_FOUND",
            "The requested API path was not found.", request_id
        );
    } else if (method != "GET") {
        send_error(
            client, 405, "METHOD_NOT_ALLOWED",
            "This endpoint supports GET requests only.", request_id
        );
    } else if (path == "/api/v1/health") {
        send_response(client, 200, "{\"status\":\"ok\"}");
    } else {
        send_error(
            client, 404, "NOT_FOUND",
            "The requested resource was not found.", request_id
        );
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
              << "Press Ctrl+C to stop.\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);

        if (client == INVALID_SOCKET) {
            std::cerr << "Failed to accept a connection.\n";
            continue;
        }

        handle_client(client);
    }

    // The loop runs until the process is stopped.
    closesocket(server);
    WSACleanup();
    return 0;
}
