#include <openssl/err.h>
#include <openssl/ssl.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string base64(const std::string& input)
{
    std::string output(4 * ((input.size() + 2) / 3), '\0');
    const int size = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(output.data()),
        reinterpret_cast<const unsigned char*>(input.data()),
        static_cast<int>(input.size()));
    output.resize(static_cast<std::size_t>(size));
    return output;
}

std::vector<std::string> fields(const std::string& line)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = line.find('\t', start);
        result.push_back(line.substr(start, end == std::string::npos
                                               ? std::string::npos
                                               : end - start));
        if (end == std::string::npos) return result;
        start = end + 1;
    }
}

void emit_error(const std::string& message, long verify_result = 0)
{
    std::cout << "ERR\t" << verify_result << "\t" << base64(message)
              << std::endl;
}

std::string ssl_error()
{
    const unsigned long code = ERR_get_error();
    if (code == 0) return "TLS operation failed without an OpenSSL error";
    char buffer[256]{};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return buffer;
}

void exchange(const std::string& line)
{
    const auto input = fields(line);
    if (input.size() != 6) {
        emit_error("request requires six tab-delimited fields");
        return;
    }
    const std::string& method = input[0];
    const std::string& path = input[1];
    const bool tls = input[2] != "plain";
    const bool trust_test_cert = input[2].find("trusted") == 0;
    const bool force_tls12 = input[2] == "trusted12";
    const bool force_tls13 = input[2] == "trusted13";
    const bool force_tls11 = input[2] == "trusted11";
    const bool force_tls10 = input[2] == "trusted10";
    const std::string& ca_file = input[3];
    const std::string& token = input[4];
    const std::string& body = input[5];

    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) {
        emit_error("socket() failed");
        return;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8443);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(socket_handle, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) == SOCKET_ERROR) {
        closesocket(socket_handle);
        emit_error("connect() failed");
        return;
    }
    DWORD timeout = 10000;
    setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(
        nullptr, SSL_CTX_free);
    std::unique_ptr<SSL, decltype(&SSL_free)> ssl(nullptr, SSL_free);
    if (tls) {
        context.reset(SSL_CTX_new(TLS_client_method()));
        if (!context) {
            const std::string error = "OpenSSL client initialization failed";
            closesocket(socket_handle);
            emit_error(error);
            return;
        }
        const int requested_version = force_tls10 ? TLS1_VERSION
            : force_tls11 ? TLS1_1_VERSION
            : force_tls12 ? TLS1_2_VERSION
            : force_tls13 ? TLS1_3_VERSION : TLS1_2_VERSION;
        if (SSL_CTX_set_min_proto_version(context.get(), requested_version) != 1 ||
            ((force_tls10 || force_tls11 || force_tls12 || force_tls13) &&
             SSL_CTX_set_max_proto_version(context.get(), requested_version) != 1)) {
            closesocket(socket_handle);
            emit_error("could not configure requested TLS protocol");
            return;
        }
        if (force_tls10 || force_tls11) SSL_CTX_set_security_level(context.get(), 0);
        SSL_CTX_set_verify(context.get(), SSL_VERIFY_PEER, nullptr);
        if (trust_test_cert) {
            if (ca_file.empty() ||
                SSL_CTX_load_verify_locations(context.get(), ca_file.c_str(),
                                              nullptr) != 1) {
                closesocket(socket_handle);
                emit_error("could not load the supplied test CA file");
                return;
            }
        } else {
            SSL_CTX_set_default_verify_paths(context.get());
        }
        ssl.reset(SSL_new(context.get()));
        if (!ssl || SSL_set1_host(ssl.get(), "localhost") != 1 ||
            SSL_set_fd(ssl.get(), static_cast<int>(socket_handle)) != 1) {
            closesocket(socket_handle);
            emit_error("could not configure OpenSSL client session");
            return;
        }
        if (SSL_connect(ssl.get()) != 1) {
            const long verify = SSL_get_verify_result(ssl.get());
            const std::string error = ssl_error();
            ssl.reset();
            closesocket(socket_handle);
            emit_error(error, verify);
            return;
        }
    }

    std::ostringstream request;
    request << method << ' ' << path << " HTTP/1.1\r\n"
            << "Host: localhost\r\n"
            << "Connection: close\r\n";
    if (!token.empty()) request << "Authorization: Bearer " << token << "\r\n";
    if (!body.empty()) {
        request << "Content-Type: application/json\r\n"
                << "Content-Length: " << body.size() << "\r\n";
    }
    request << "\r\n" << body;
    const std::string wire = request.str();

    std::size_t sent = 0;
    while (sent < wire.size()) {
        const int count = tls
            ? SSL_write(ssl.get(), wire.data() + sent,
                        static_cast<int>(wire.size() - sent))
            : send(socket_handle, wire.data() + sent,
                   static_cast<int>(wire.size() - sent), 0);
        if (count <= 0) {
            ssl.reset();
            closesocket(socket_handle);
            emit_error(tls ? "OpenSSL request write failed"
                           : "plain HTTP request write failed");
            return;
        }
        sent += static_cast<std::size_t>(count);
    }

    std::string response;
    char buffer[4096];
    for (;;) {
        const int count = tls
            ? SSL_read(ssl.get(), buffer, static_cast<int>(sizeof(buffer)))
            : recv(socket_handle, buffer, static_cast<int>(sizeof(buffer)), 0);
        if (count <= 0) break;
        response.append(buffer, static_cast<std::size_t>(count));
    }
    const std::string protocol = tls ? SSL_get_version(ssl.get()) : "plaintext";
    const std::string cipher = tls ? SSL_get_cipher_name(ssl.get()) : "none";
    ssl.reset();
    closesocket(socket_handle);

    const std::size_t status_end = response.find("\r\n");
    int status = 0;
    if (status_end != std::string::npos) {
        std::istringstream status_line(response.substr(0, status_end));
        std::string version;
        status_line >> version >> status;
    }
    const std::size_t body_start = response.find("\r\n\r\n");
    const std::string response_body = body_start == std::string::npos
        ? std::string{} : response.substr(body_start + 4);
    if (status == 0) {
        emit_error("no HTTP response received");
        return;
    }
    std::cout << "OK\t" << status << '\t' << protocol << '\t' << cipher
              << '\t' << base64(response_body) << std::endl;
}

} // namespace

int main()
{
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 2;
    std::string line;
    while (std::getline(std::cin, line)) exchange(line);
    WSACleanup();
    return 0;
}
