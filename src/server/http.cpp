#include "server/http.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#define NPC_CLOSE closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#define NPC_CLOSE ::close
#endif

namespace npc::server {

namespace {
constexpr std::size_t kMaxRequest = 8 * 1024 * 1024;

const char* status_text(int s) {
    switch (s) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
    }
    return "OK";
}

bool send_all(std::intptr_t fd, const char* data, std::size_t len) {
    while (len > 0) {
#ifdef _WIN32
        const int n = ::send(static_cast<SOCKET>(fd), data, static_cast<int>(std::min<std::size_t>(len, 1 << 20)), 0);
#else
        const ssize_t n = ::send(static_cast<int>(fd), data, len, MSG_NOSIGNAL);
#endif
        if (n <= 0) return false;
        data += n;
        len -= static_cast<std::size_t>(n);
    }
    return true;
}
} // namespace

std::string Request::param(const std::string& key, const std::string& fallback) const {
    std::size_t pos = 0;
    while (pos <= query.size()) {
        const auto amp = query.find('&', pos);
        const std::string part = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        const auto eq = part.find('=');
        if (HttpServer::url_decode(part.substr(0, eq)) == key) return eq == std::string::npos ? "" : HttpServer::url_decode(part.substr(eq + 1));
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return fallback;
}

Response Response::json(std::string body, int status) {
    Response r;
    r.status = status;
    r.body = std::move(body);
    return r;
}
Response Response::text(std::string body, int status) {
    Response r;
    r.status = status;
    r.content_type = "text/plain; charset=utf-8";
    r.body = std::move(body);
    return r;
}
Response Response::error(int status, const std::string& message) {
    std::string esc;
    for (char c : message) {
        if (c == '"' || c == '\\') esc += '\\';
        if (static_cast<unsigned char>(c) >= 0x20) esc += c;
    }
    return json("{\"error\": \"" + esc + "\"}", status);
}

std::string HttpServer::url_decode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) && std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else out += s[i];
    }
    return out;
}

std::string HttpServer::mime_type(const std::string& path) {
    const auto ext = std::filesystem::path(path).extension().string();
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".csv") return "text/csv; charset=utf-8";
    return "application/octet-stream";
}

bool HttpServer::parse_request(const std::string& raw, Request& out) {
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;
    std::istringstream head(raw.substr(0, header_end));
    std::string line;
    if (!std::getline(head, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream first(line);
    std::string target, version;
    first >> out.method >> target >> version;
    if (out.method.empty() || target.empty() || target[0] != '/') return false;
    const auto q = target.find('?');
    out.path = url_decode(target.substr(0, q));
    out.query = q == std::string::npos ? "" : target.substr(q + 1);
    while (std::getline(head, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::string value = line.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());
        out.headers[key] = value;
    }
    out.body = raw.substr(header_end + 4);
    return true;
}

HttpServer::~HttpServer() { stop(); }

void HttpServer::route(const std::string& method, const std::string& path, Handler h) { routes_[method + " " + path] = std::move(h); }

void HttpServer::static_dir(const std::string& prefix, const std::string& root) { statics_.emplace_back(prefix, root); }

bool HttpServer::start(const std::string& host, int port, std::string& error) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { error = "WSAStartup failed"; return false; }
#endif
    const auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { error = "socket() failed"; return false; }
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) { error = "invalid host " + host; NPC_CLOSE(fd); return false; }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        error = "cannot bind " + host + ":" + std::to_string(port) + " (port in use?)";
        NPC_CLOSE(fd);
        return false;
    }
    if (::listen(fd, 64) != 0) { error = "listen() failed"; NPC_CLOSE(fd); return false; }
    socklen_t len = sizeof addr;
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    port_ = ntohs(addr.sin_port);
    listen_fd_ = static_cast<std::intptr_t>(fd);
    running_ = true;
    thread_ = std::thread([this] { accept_loop(); });
    return true;
}

void HttpServer::stop() {
    if (!running_.exchange(false)) return;
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(listen_fd_));
#else
    ::shutdown(static_cast<int>(listen_fd_), SHUT_RDWR);
    ::close(static_cast<int>(listen_fd_));
#endif
    if (thread_.joinable()) thread_.join();
    for (int i = 0; i < 200 && active_ > 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void HttpServer::accept_loop() {
    while (running_) {
        sockaddr_in client{};
        socklen_t len = sizeof client;
#ifdef _WIN32
        const auto c = ::accept(static_cast<SOCKET>(listen_fd_), reinterpret_cast<sockaddr*>(&client), &len);
        if (c == INVALID_SOCKET) { if (!running_) break; continue; }
#else
        const int c = ::accept(static_cast<int>(listen_fd_), reinterpret_cast<sockaddr*>(&client), &len);
        if (c < 0) { if (!running_) break; continue; }
#endif
        ++active_;
        std::thread([this, c] {
            handle(static_cast<std::intptr_t>(c));
            --active_;
        }).detach();
    }
}

void HttpServer::handle(std::intptr_t client) {
    std::string raw;
    char buf[16384];
    std::size_t needed = 0;
    while (true) {
#ifdef _WIN32
        const int n = ::recv(static_cast<SOCKET>(client), buf, sizeof buf, 0);
#else
        const ssize_t n = ::recv(static_cast<int>(client), buf, sizeof buf, 0);
#endif
        if (n <= 0) break;
        raw.append(buf, static_cast<std::size_t>(n));
        if (raw.size() > kMaxRequest) break;
        const auto he = raw.find("\r\n\r\n");
        if (he != std::string::npos) {
            if (needed == 0) {
                Request tmp;
                parse_request(raw, tmp);
                const auto it = tmp.headers.find("content-length");
                needed = he + 4 + (it != tmp.headers.end() ? static_cast<std::size_t>(std::strtoull(it->second.c_str(), nullptr, 10)) : 0);
            }
            if (raw.size() >= needed) break;
        }
    }
    Request req;
    Response res;
    if (raw.size() > kMaxRequest) res = Response::error(413, "request too large");
    else if (!parse_request(raw, req)) res = Response::error(400, "malformed request");
    else {
        try {
            res = dispatch(req);
        } catch (const std::exception& e) {
            res = Response::error(500, e.what());
        }
    }
    std::ostringstream head;
    head << "HTTP/1.1 " << res.status << ' ' << status_text(res.status) << "\r\n"
         << "Content-Type: " << res.content_type << "\r\n"
         << "Content-Length: " << res.body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Connection: close\r\n";
    for (const auto& [k, v] : res.headers) head << k << ": " << v << "\r\n";
    head << "\r\n";
    const std::string h = head.str();
    if (send_all(client, h.data(), h.size())) send_all(client, res.body.data(), res.body.size());
#ifdef _WIN32
    ::shutdown(static_cast<SOCKET>(client), SD_SEND);
#else
    ::shutdown(static_cast<int>(client), SHUT_WR);
#endif
    NPC_CLOSE(static_cast<decltype(::socket(0, 0, 0))>(client));
}

Response HttpServer::dispatch(const Request& req) {
    if (req.method == "OPTIONS") {
        Response r = Response::text("", 204);
        r.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        r.headers["Access-Control-Allow-Headers"] = "Content-Type";
        return r;
    }
    if (const auto it = routes_.find(req.method + " " + req.path); it != routes_.end()) return it->second(req);
    if (req.method == "GET") {
        for (const auto& [prefix, root] : statics_) {
            if (req.path.rfind(prefix, 0) != 0) continue;
            std::string rel = req.path.substr(prefix.size());
            if (rel.empty() || rel.back() == '/') rel += "index.html";
            if (rel.find("..") != std::string::npos) return Response::error(400, "invalid path");
            const std::filesystem::path file = std::filesystem::path(root) / rel;
            std::ifstream in(file, std::ios::binary);
            if (!in) continue;
            std::ostringstream ss;
            ss << in.rdbuf();
            Response r;
            r.content_type = mime_type(file.string());
            r.body = ss.str();
            return r;
        }
    }
    for (const auto& [key, h] : routes_)
        if (key.substr(key.find(' ') + 1) == req.path) return Response::error(405, "method not allowed");
    return Response::error(404, "not found: " + req.path);
}

} // namespace npc::server
