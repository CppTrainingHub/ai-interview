#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

#include "net/http_interview_server.h"
#include "net/interview_server.h"

namespace interview {
namespace net {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

constexpr int kReceiveBufferSize = 4096;

struct ParsedHttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string SocketErrorMessage(const std::string& prefix) {
#ifdef _WIN32
    return prefix + ": winsock error " + std::to_string(WSAGetLastError());
#else
    return prefix + ": " + std::strerror(errno);
#endif
}

void CloseSocket(SocketHandle socket) {
    if (socket == kInvalidSocket) {
        return;
    }

#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

std::string Trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

std::string JsonError(const std::string& message) {
    nlohmann::json json;
    json["ok"] = false;
    json["error"] = message;
    return json.dump(2);
}

std::string ReasonPhrase(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 500:
            return "Internal Server Error";
        default:
            return "OK";
    }
}

std::string BuildHttpResponse(int status_code, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << ReasonPhrase(status_code) << "\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             << "Access-Control-Allow-Headers: Content-Type\r\n"
             << "\r\n"
             << body;
    return response.str();
}

std::size_t ContentLength(const std::map<std::string, std::string>& headers) {
    const auto it = headers.find("content-length");
    if (it == headers.end() || it->second.empty()) {
        return 0;
    }

    return static_cast<std::size_t>(std::stoul(it->second));
}

std::size_t ContentLengthFromHeaderText(const std::string& header_text) {
    std::istringstream header_stream(header_text);
    std::string line;
    std::getline(header_stream, line);

    std::map<std::string, std::string> headers;
    while (std::getline(header_stream, line)) {
        line = Trim(line);
        if (line.empty()) {
            continue;
        }

        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        headers[ToLower(Trim(line.substr(0, separator)))] = Trim(line.substr(separator + 1));
    }

    return ContentLength(headers);
}

ParsedHttpRequest ParseHttpRequest(const std::string& raw_request) {
    const auto header_end = raw_request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        throw std::runtime_error("invalid HTTP request: missing header terminator");
    }

    ParsedHttpRequest request;
    std::istringstream header_stream(raw_request.substr(0, header_end));

    std::string request_line;
    if (!std::getline(header_stream, request_line)) {
        throw std::runtime_error("invalid HTTP request: missing request line");
    }
    request_line = Trim(request_line);

    std::istringstream request_line_stream(request_line);
    std::string http_version;
    request_line_stream >> request.method >> request.path >> http_version;
    if (request.method.empty() || request.path.empty() || http_version.empty()) {
        throw std::runtime_error("invalid HTTP request line");
    }

    std::string line;
    while (std::getline(header_stream, line)) {
        line = Trim(line);
        if (line.empty()) {
            continue;
        }

        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            throw std::runtime_error("invalid HTTP header: " + line);
        }

        const auto key = ToLower(Trim(line.substr(0, separator)));
        const auto value = Trim(line.substr(separator + 1));
        request.headers[key] = value;
    }

    request.body = raw_request.substr(header_end + 4);
    const auto expected_body_size = ContentLength(request.headers);
    if (request.body.size() < expected_body_size) {
        throw std::runtime_error("invalid HTTP request: body shorter than Content-Length");
    }
    if (request.body.size() > expected_body_size) {
        request.body.resize(expected_body_size);
    }

    return request;
}

std::string PathWithoutQuery(std::string path) {
    const auto query_start = path.find('?');
    if (query_start != std::string::npos) {
        path.resize(query_start);
    }
    return path;
}

bool IsOkJson(const std::string& body) {
    try {
        const auto json = nlohmann::json::parse(body);
        return json.value("ok", false);
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

class HttpInterviewServer::Impl {
public:
    Impl(config::AppConfig app_config, HttpServerConfig http_config)
        : server(std::move(app_config)),
          config(std::move(http_config)) {
        ValidateConfig();
    }

    ~Impl() {
        Stop();
    }

    void Run() {
        InitializeSockets();
        listen_socket = CreateListenSocket();
        running = true;

        while (running) {
            sockaddr_storage client_address{};
            socklen_t client_address_size = sizeof(client_address);
            const SocketHandle client_socket = accept(
                listen_socket,
                reinterpret_cast<sockaddr*>(&client_address),
                &client_address_size);

            if (client_socket == kInvalidSocket) {
                if (!running) {
                    break;
                }
                continue;
            }

            try {
                const std::string request = ReadRequest(client_socket);
                const std::string response = HandleRawRequest(request);
                SendAll(client_socket, response);
            } catch (const std::exception& e) {
                const std::string response = BuildHttpResponse(400, JsonError(e.what()));
                SendAll(client_socket, response);
            }

            CloseSocket(client_socket);
        }

        CloseSocket(listen_socket);
        listen_socket = kInvalidSocket;
        CleanupSockets();
    }

    void Stop() {
        running = false;
        CloseSocket(listen_socket);
        listen_socket = kInvalidSocket;
    }

    HttpServerConfig Config() const {
        return config;
    }

    std::string HandleRawRequest(const std::string& raw_request) {
        try {
            const auto request = ParseHttpRequest(raw_request);
            return Dispatch(request);
        } catch (const std::exception& e) {
            return BuildHttpResponse(400, JsonError(e.what()));
        }
    }

private:
    void ValidateConfig() const {
        if (config.host.empty()) {
            throw std::runtime_error("HTTP listen host must not be empty");
        }
        if (config.port <= 0 || config.port > 65535) {
            throw std::runtime_error("HTTP listen port must be between 1 and 65535");
        }
        if (config.backlog <= 0) {
            throw std::runtime_error("HTTP backlog must be positive");
        }
    }

    void InitializeSockets() {
#ifdef _WIN32
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            throw std::runtime_error("failed to initialize winsock");
        }
#endif
    }

    void CleanupSockets() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    SocketHandle CreateListenSocket() const {
        const SocketHandle socket_handle = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_handle == kInvalidSocket) {
            throw std::runtime_error(SocketErrorMessage("failed to create listen socket"));
        }

        int reuse_address = 1;
        setsockopt(socket_handle,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse_address),
                   sizeof(reuse_address));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<std::uint16_t>(config.port));

        if (config.host == "0.0.0.0") {
            address.sin_addr.s_addr = htonl(INADDR_ANY);
        } else if (inet_pton(AF_INET, config.host.c_str(), &address.sin_addr) != 1) {
            CloseSocket(socket_handle);
            throw std::runtime_error("HTTP listen host must be an IPv4 address, for example 0.0.0.0 or 127.0.0.1");
        }

        if (bind(socket_handle,
                 reinterpret_cast<const sockaddr*>(&address),
                 sizeof(address)) != 0) {
            CloseSocket(socket_handle);
            throw std::runtime_error(SocketErrorMessage("failed to bind listen socket"));
        }

        if (listen(socket_handle, config.backlog) != 0) {
            CloseSocket(socket_handle);
            throw std::runtime_error(SocketErrorMessage("failed to listen on socket"));
        }

        return socket_handle;
    }

    std::string ReadRequest(SocketHandle client_socket) const {
        std::string request;
        std::string header_part;
        std::size_t expected_body_size = 0;
        bool header_complete = false;

        char buffer[kReceiveBufferSize];
        while (true) {
#ifdef _WIN32
            const int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
#else
            const ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
#endif
            if (bytes_read <= 0) {
                break;
            }

            request.append(buffer, static_cast<std::size_t>(bytes_read));

            const auto header_end = request.find("\r\n\r\n");
            if (!header_complete && header_end != std::string::npos) {
                header_complete = true;
                header_part = request.substr(0, header_end + 4);
                expected_body_size = ContentLengthFromHeaderText(header_part);
            }

            if (header_complete) {
                const auto body_start = request.find("\r\n\r\n") + 4;
                if (request.size() >= body_start + expected_body_size) {
                    break;
                }
            }
        }

        if (request.empty()) {
            throw std::runtime_error("empty HTTP request");
        }

        return request;
    }

    void SendAll(SocketHandle client_socket, const std::string& response) const {
        std::size_t sent = 0;
        while (sent < response.size()) {
#ifdef _WIN32
            const int result = send(client_socket,
                                    response.data() + sent,
                                    static_cast<int>(response.size() - sent),
                                    0);
#else
            const ssize_t result = send(client_socket,
                                        response.data() + sent,
                                        response.size() - sent,
                                        0);
#endif
            if (result <= 0) {
                throw std::runtime_error(SocketErrorMessage("failed to send HTTP response"));
            }
            sent += static_cast<std::size_t>(result);
        }
    }

    std::string Dispatch(const ParsedHttpRequest& request) {
        if (request.method == "OPTIONS") {
            return BuildHttpResponse(200, "{}");
        }

        const auto path = PathWithoutQuery(request.path);
        if (request.method == "GET" && path == "/health") {
            return BuildHttpResponse(200, server.HandleHealth());
        }

        if (request.method != "POST") {
            return BuildHttpResponse(405, JsonError("method not allowed"));
        }

        std::string body;
        if (path == "/interview/start") {
            body = server.HandleStart(request.body);
        } else if (path == "/interview/answer") {
            body = server.HandleAnswer(request.body);
        } else if (path == "/interview/report") {
            body = server.HandleReport(request.body);
        } else {
            return BuildHttpResponse(404, JsonError("route not found: " + path));
        }

        return BuildHttpResponse(IsOkJson(body) ? 200 : 400, body);
    }

    InterviewServer server;
    HttpServerConfig config;
    bool running = false;
    SocketHandle listen_socket = kInvalidSocket;
};

HttpInterviewServer::HttpInterviewServer(config::AppConfig app_config,
                                         HttpServerConfig http_config)
    : pImpl(std::make_unique<Impl>(std::move(app_config), std::move(http_config))) {}

HttpInterviewServer::~HttpInterviewServer() = default;

HttpInterviewServer::HttpInterviewServer(HttpInterviewServer&&) noexcept = default;

HttpInterviewServer& HttpInterviewServer::operator=(HttpInterviewServer&&) noexcept = default;

void HttpInterviewServer::Run() {
    pImpl->Run();
}

void HttpInterviewServer::Stop() {
    pImpl->Stop();
}

HttpServerConfig HttpInterviewServer::Config() const {
    return pImpl->Config();
}

std::string HttpInterviewServer::HandleRawRequest(const std::string& raw_request) {
    return pImpl->HandleRawRequest(raw_request);
}

} // namespace net
} // namespace interview
