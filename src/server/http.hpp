#pragma once
// Minimal HTTP/1.1 server (no dependencies). One short-lived thread per connection, "Connection: close".
// Intended for localhost use by the visualisation client.

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace npc::server {

struct Request {
    std::string method, path, query, body;
    std::map<std::string, std::string> headers;
    std::string param(const std::string& key, const std::string& fallback = "") const;
};

struct Response {
    int status = 200;
    std::string content_type = "application/json; charset=utf-8";
    std::string body;
    std::map<std::string, std::string> headers;
    static Response json(std::string body, int status = 200);
    static Response text(std::string body, int status = 200);
    static Response error(int status, const std::string& message);
};

using Handler = std::function<Response(const Request&)>;

class HttpServer {
public:
    ~HttpServer();
    void route(const std::string& method, const std::string& path, Handler h);
    /// Serves files under `root` for GET requests that match no route ("/" -> index.html).
    void static_dir(const std::string& url_prefix, const std::string& root);
    /// Binds and starts accepting in a background thread. Returns false with an error message.
    bool start(const std::string& host, int port, std::string& error);
    void stop();
    int port() const { return port_; }

    /// Exposed for tests: parse a raw request.
    static bool parse_request(const std::string& raw, Request& out);
    static std::string url_decode(const std::string& s);
    static std::string mime_type(const std::string& path);

private:
    void accept_loop();
    void handle(std::intptr_t client);
    Response dispatch(const Request& req);

    std::map<std::string, Handler> routes_; // "GET /api/state"
    std::vector<std::pair<std::string, std::string>> statics_;
    std::intptr_t listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<int> active_{0};
    std::thread thread_;
    int port_ = 0;
};

} // namespace npc::server
