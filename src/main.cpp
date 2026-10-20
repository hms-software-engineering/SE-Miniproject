
#include "api/api.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr unsigned short PORT = 8080;
constexpr std::size_t MAX_REQUEST_SIZE = 1024 * 1024;
constexpr std::size_t MAX_HEADER_SIZE = 16 * 1024;

std::string status_text(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default:  return "Error";
    }
}

std::string make_request_id()
{
    static unsigned long long counter = 0;
    return "req-" + std::to_string(++counter);
}

std::string to_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

bool send_all(SOCKET client, const std::string& data)
{
    std::size_t sent = 0;

    while (sent < data.size()) {
        const std::size_t remaining = data.size() - sent;
        const int chunk = static_cast<int>(
            std::min<std::size_t>(remaining, 64 * 1024));

        const int result = send(
            client, data.data() + sent, chunk, 0);

        if (result == SOCKET_ERROR || result == 0)
            return false;

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

void send_response(
    SOCKET client,
    int status,
    const std::string& body)
{
    std::ostringstream response;

    response << "HTTP/1.1 " << status << ' '
             << status_text(status) << "\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    if (!send_all(client, response.str())) {
        std::cerr << "Failed to send HTTP response: "
                  << WSAGetLastError() << '\n';
    }
}

void send_error(
    SOCKET client,
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& request_id)
{
    const hms::ApiError error{
        status, code, message, request_id, ""
    };

    send_response(client, status, hms::make_error_json(error));
}

void handle_client(SOCKET client)
{
    const std::string request_id = make_request_id();
    std::string raw;
    char buffer[4096];

    std::size_t header_end = std::string::npos;
    std::size_t content_length = 0;
    bool has_content_length = false;

    // Read headers and the declared body, subject to a size limit.
    while (raw.size() < MAX_REQUEST_SIZE) {
        const int received = recv(
            client, buffer, static_cast<int>(sizeof(buffer)), 0);

        if (received == SOCKET_ERROR) {
            std::cerr << "recv() failed: "
                      << WSAGetLastError() << '\n';
            return;
        }

        if (received == 0)
            break;

        raw.append(buffer, static_cast<std::size_t>(received));

        header_end = raw.find("\r\n\r\n");

        if (header_end == std::string::npos) {
            if (raw.size() > MAX_HEADER_SIZE) {
                send_error(client, 431, "HEADERS_TOO_LARGE",
                           "Request headers are too large.", request_id);
                return;
            }
            continue;
        }

        if (header_end > MAX_HEADER_SIZE) {
            send_error(client, 431, "HEADERS_TOO_LARGE",
                       "Request headers are too large.", request_id);
            return;
        }

        std::istringstream headers(raw.substr(0, header_end));
        std::string line;
        std::getline(headers, line); // Request line

        bool invalid_length = false;
        bool transfer_encoding = false;

        while (std::getline(headers, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const auto colon = line.find(':');
            if (colon == std::string::npos) {
                if (!line.empty()) {
                    send_error(client, 400, "BAD_REQUEST",
                               "Malformed HTTP header.", request_id);
                    return;
                }
                continue;
            }

            const std::string name =
                to_lower(line.substr(0, colon));

            std::string value = line.substr(colon + 1);
            const auto first = value.find_first_not_of(" \t");

            value = first == std::string::npos
                ? "" : value.substr(first);

            if (name == "content-length") {
                if (has_content_length) {
                    invalid_length = true;
                    break;
                }

                try {
                    std::size_t parsed = 0;
                    const auto length = std::stoull(value, &parsed);

                    if (parsed != value.size() ||
                        length > MAX_REQUEST_SIZE) {
                        invalid_length = true;
                        break;
                    }

                    content_length = static_cast<std::size_t>(length);
                    has_content_length = true;
                } catch (...) {
                    invalid_length = true;
                    break;
                }
            }

            if (name == "transfer-encoding")
                transfer_encoding = true;
        }

        if (invalid_length) {
            send_error(client, 400, "BAD_REQUEST",
                       "Invalid or duplicate Content-Length.",
                       request_id);
            return;
        }

        if (transfer_encoding) {
            send_error(client, 400, "BAD_REQUEST",
                       "Transfer-Encoding is not supported.",
                       request_id);
            return;
        }

        const std::size_t body_start = header_end + 4;

        if (content_length > MAX_REQUEST_SIZE - body_start) {
            send_error(client, 413, "PAYLOAD_TOO_LARGE",
                       "Request is too large.", request_id);
            return;
        }

        if (raw.size() >= body_start + content_length)
            break;
    }

    if (header_end == std::string::npos) {
        send_error(client, 400, "BAD_REQUEST",
                   "Incomplete HTTP headers.", request_id);
        return;
    }

    std::istringstream request(raw.substr(0, header_end));
    std::string method, path, version;
    request >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty()) {
        send_error(client, 400, "BAD_REQUEST",
                   "Invalid HTTP request line.", request_id);
        return;
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        send_error(client, 400, "BAD_REQUEST",
                   "Unsupported HTTP version.", request_id);
        return;
    }

    const auto query_pos = path.find('?');
    if (query_pos != std::string::npos)
        path.resize(query_pos);

    std::cout << "Request: " << method << ' ' << path << '\n';
    hms::log_request(method, path, request_id);

    if (path != hms::API_BASE_PATH &&
        path.rfind(std::string(hms::API_BASE_PATH) + "/", 0) != 0) {
        send_error(client, 404, "NOT_FOUND",
                   "The requested API path was not found.", request_id);
        return;
    }

    if (method == "GET" && path == "/api/v1/health") {
        send_response(client, 200, "{\"status\":\"ok\"}");
        return;
    }

    if (method == "POST" && path == "/api/v1/auth/login") {
        send_error(client, 501, "AUTH_NOT_IMPLEMENTED",
                   "Authentication is not configured on this server.",
                   request_id);
        return;
    }

    if (method != "GET" && method != "POST") {
        send_error(client, 405, "METHOD_NOT_ALLOWED",
                   "The HTTP method is not supported.", request_id);
        return;
    }

    send_error(client, 404, "NOT_FOUND",
               "The requested resource was not found.", request_id);
}

} // namespace

int main()
{
    // Keep diagnostics visible in the console.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "Starting HMS server...\n";

    WSADATA wsa_data{};

    const int startup = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup != 0) {
        std::cerr << "WSAStartup failed: " << startup << '\n';
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "socket() failed: "
                  << WSAGetLastError() << '\n';
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
        std::cerr << "inet_pton() failed.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: "
                  << WSAGetLastError() << '\n';
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed: "
                  << WSAGetLastError() << '\n';
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "HMS API listening at http://127.0.0.1:"
              << PORT << "/api/v1\n"
              << "Health endpoint: GET /api/v1/health\n"
              << "Stop with Ctrl+C or close this console.\n";

    // Keep this simple server synchronous. Closing the console terminates
    // the process; no unsafe signal handler is needed.
    for (;;) {
        std::cout << "Waiting for a client...\n";

        SOCKET client = accept(server, nullptr, nullptr);

        if (client == INVALID_SOCKET) {
            std::cerr << "accept() failed: "
                      << WSAGetLastError() << '\n';
            break;
        }

        std::cout << "Accepted client connection.\n";

        // Ensure the socket closes even if request handling throws.
        try {
            handle_client(client);
        } catch (const std::exception& e) {
            std::cerr << "Request handler exception: "
                      << e.what() << '\n';

            try {
                send_error(client, 500, "INTERNAL_ERROR",
                           "Internal server error.", make_request_id());
            } catch (...) {
                // Do not allow an error response failure to escape.
            }
        } catch (...) {
            std::cerr << "Unknown request handler exception.\n";
        }

        shutdown(client, SD_BOTH);
        closesocket(client);

        std::cout << "Client connection closed.\n";
    }

    closesocket(server);
    WSACleanup();

    std::cout << "HMS server stopped.\n";
    return 0;
}