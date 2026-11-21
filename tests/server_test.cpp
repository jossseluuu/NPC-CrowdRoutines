#include "server/api.hpp"
#include "server/http.hpp"
#include "server/runner.hpp"
#include "test_framework.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace npc;

TEST_CASE("server/request parsing and url decoding") {
    server::Request r;
    CHECK(server::HttpServer::parse_request("GET /api/agent?id=12&x=a%20b HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n", r));
    CHECK(r.method == "GET");
    CHECK(r.path == "/api/agent");
    CHECK(r.param("id") == "12");
    CHECK(r.param("x") == "a b");
    CHECK(r.param("missing", "d") == "d");
    CHECK(r.headers["host"] == "x");
    CHECK(!server::HttpServer::parse_request("garbage", r));
    CHECK(server::HttpServer::mime_type("a/b/index.html").find("text/html") == 0);
}

#ifndef _WIN32
namespace {
struct Reply {
    int status = 0;
    std::string headers, body;
};
Reply http(int port, const std::string& method, const std::string& path, const std::string& body = "") {
    Reply rep;
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) { ::close(fd); return rep; }
    const std::string req = method + " " + path + " HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: " +
                            std::to_string(body.size()) + "\r\n\r\n" + body;
    ::send(fd, req.data(), req.size(), 0);
    std::string raw;
    char buf[65536];
    ssize_t n;
    while ((n = ::recv(fd, buf, sizeof buf, 0)) > 0) raw.append(buf, static_cast<std::size_t>(n));
    ::close(fd);
    const auto he = raw.find("\r\n\r\n");
    if (he == std::string::npos) return rep;
    rep.headers = raw.substr(0, he);
    rep.body = raw.substr(he + 4);
    rep.status = std::atoi(raw.substr(9, 3).c_str());
    return rep;
}
} // namespace

TEST_CASE("server/end to end API over HTTP") {
    data::Catalog cat;
    world::MapLibrary lib;
    std::string err;
    REQUIRE(cat.load(NPC_DATA_DIR, err) && lib.load(NPC_DATA_DIR, cat, err));
    server::SimRunner runner(cat, lib);
    sim::Config cfg;
    cfg.population = 700;
    cfg.seed = 3;
    REQUIRE(runner.rebuild([&](sim::Simulation& s, std::string& e) { return s.reset(cfg, e); }, err));
    runner.set_speed(600);
    runner.set_running(true);
    runner.start();
    server::HttpServer srv;
    const std::string tmp_runs = std::string(NPC_DATA_DIR) + "/../build-test-runs";
    server::AppPaths paths{NPC_DATA_DIR, std::string(NPC_DATA_DIR) + "/../web", tmp_runs};
    server::register_api(srv, runner, paths, cat, lib);
    REQUIRE(srv.start("127.0.0.1", 0, err));
    const int port = srv.port();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    auto info = http(port, "GET", "/api/info");
    CHECK(info.status == 200);
    const auto infoj = json::parse(info.body).value;
    CHECK(infoj["templates"].size() >= 6);
    CHECK(infoj["languages"].size() >= 2);

    auto state = http(port, "GET", "/api/state");
    CHECK(state.status == 200);
    const auto st = json::parse(state.body).value;
    CHECK(st.integer("population", 0) == 700);
    CHECK(st["clock"].num("tick", 0) > 0); // it is running

    auto frame = http(port, "GET", "/api/frame");
    CHECK(frame.status == 200);
    CHECK(frame.body.size() > 700 * 12);
    CHECK(frame.body.substr(0, 4) == "NPCF");

    CHECK(http(port, "GET", "/api/map").body.find("npc-city-map") != std::string::npos);
    CHECK(json::parse(http(port, "GET", "/api/catalog").body).value["activities"].size() == cat.activities.size());
    CHECK(json::parse(http(port, "GET", "/api/i18n?lang=en").body).value.str("lang") == "en");
    CHECK(http(port, "GET", "/api/i18n?lang=xx").status == 404);

    auto agent = http(port, "GET", "/api/agent?id=5");
    CHECK(agent.status == 200);
    CHECK(!json::parse(agent.body).value.str("name").empty());
    CHECK(http(port, "GET", "/api/agent?id=99999").status == 404);
    CHECK(http(port, "GET", "/api/building?id=0").status == 200);

    auto q = http(port, "POST", "/api/query", R"({"rules":[{"field":"age","min":65}]})");
    CHECK(q.status == 200);
    CHECK(q.body.size() == (700 + 7) / 8);
    CHECK(q.headers.find("X-Matches:") != std::string::npos);

    CHECK(http(port, "POST", "/api/command", R"({"type":"inject_event","event":"fire"})").status == 200);
    CHECK(http(port, "POST", "/api/command", R"({"type":"inject_event","event":"meteor"})").status == 400);
    CHECK(http(port, "POST", "/api/command", "{broken").status == 400);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto st2 = json::parse(http(port, "GET", "/api/state").body).value;
    CHECK(st2["events"].size() + st2["event_history"].size() >= 1);

    CHECK(json::parse(http(port, "POST", "/api/control", R"({"action":"pause"})").body).value.boolean("running", true) == false);
    CHECK(http(port, "POST", "/api/control", R"({"action":"fly"})").status == 400);
    CHECK(http(port, "GET", "/api/timeseries?max=50").status == 200);
    CHECK(http(port, "POST", "/api/export", R"({"rules":[]})").body.find("id,name,age") == 0);

    // Saved run round trip with replay verification.
    auto saved = http(port, "POST", "/api/run/save", R"({"name":"server_test"})");
    CHECK(saved.status == 200);
    auto loaded = http(port, "POST", "/api/run/load", R"({"file":"server_test.json","replay":true})");
    CHECK(loaded.status == 200);
    CHECK(json::parse(loaded.body).value.boolean("checksum_ok", false));

    // New city with another template.
    auto reset = http(port, "POST", "/api/reset", R"({"map_template":"coastal","map_size":"small","population":400,"seed":9})");
    CHECK(reset.status == 200);
    CHECK(json::parse(reset.body).value["map"].str("template") == "coastal");
    CHECK(http(port, "POST", "/api/reset", R"({"map_template":"atlantis"})").status == 400);
    CHECK(http(port, "GET", "/api/nope").status == 404);

    srv.stop();
    runner.stop();
    std::error_code ec;
    std::filesystem::remove_all(tmp_runs, ec);
}
#endif
