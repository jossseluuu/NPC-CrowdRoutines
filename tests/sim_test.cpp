#include "events/event_system.hpp"
#include "sim/simulation.hpp"
#include "test_framework.hpp"

#include <chrono>
#include <cmath>
#include <map>
#include <set>

using namespace npc;

namespace {
struct Env {
    data::Catalog cat;
    world::MapLibrary lib;
    bool ok = false;
    Env() {
        std::string err;
        ok = cat.load(NPC_DATA_DIR, err) && lib.load(NPC_DATA_DIR, cat, err);
        if (!ok) std::printf("  env error: %s\n", err.c_str());
    }
};
Env& env() {
    static Env e;
    return e;
}

sim::Config small_config(std::uint64_t seed = 7, int population = 1500) {
    sim::Config c;
    c.seed = seed;
    c.population = population;
    c.map_template = "grid";
    c.map_size = "small";
    c.start_hour = 6.0;
    return c;
}

/// Every resident is in a valid state and position.
bool consistent(const sim::Simulation& sim, std::string& why) {
    const auto& s = sim.state();
    const auto& a = s.agents;
    const auto& cat = s.cat;
    std::vector<std::uint32_t> occupancy(s.map.buildings.size(), 0);
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        if (a.activity[i] >= cat.activities.size() || a.substate[i] >= cat.substates.size()) { why = "bad activity index"; return false; }
        if (cat.substates[a.substate[i]].activity != a.activity[i]) {
            why = "substate " + cat.substates[a.substate[i]].id + " does not belong to " + cat.activities[a.activity[i]].id + " (resident " + std::to_string(i) + ")";
            return false;
        }
        if (!std::isfinite(a.pos[i].x) || !std::isfinite(a.pos[i].y) || a.pos[i].x < -50 || a.pos[i].y < -50 ||
            a.pos[i].x > s.map.width + 50 || a.pos[i].y > s.map.height + 50) {
            why = "resident " + std::to_string(i) + " out of map";
            return false;
        }
        for (float n : a.needs[i]) if (!(n >= 0.f && n <= 1.f)) { why = "need out of range"; return false; }
        if (a.move[i].active && a.location[i] != pop::kNone) { why = "moving resident still inside a building"; return false; }
        if (a.location[i] != pop::kNone) ++occupancy[a.location[i]];
        if (a.activity[i] == cat.act_travelling && !a.move[i].active && a.partner[i] == pop::kNone && a.next_decision[i] > s.now() + 600) {
            why = "resident " + std::to_string(i) + " stuck travelling without a trip";
            return false;
        }
    }
    for (std::size_t b = 0; b < occupancy.size(); ++b)
        if (occupancy[b] != s.occupancy[b]) { why = "occupancy counter mismatch at building " + std::to_string(b); return false; }
    return true;
}
} // namespace

TEST_CASE("sim/a full day keeps every resident consistent and the city alive") {
    auto& e = env();
    REQUIRE(e.ok);
    sim::Simulation sim(e.cat, e.lib);
    std::string err;
    REQUIRE(sim.reset(small_config(), err));
    const auto& s = sim.state();
    const auto& cat = e.cat;
    std::map<int, std::vector<std::uint32_t>> by_hour;
    const auto t0 = std::chrono::steady_clock::now();
    std::string why;
    bool ok = true;
    for (int h = 0; h < 24 && ok; ++h) {
        sim.run_ticks(3600);
        ok = consistent(sim, why);
        std::vector<std::uint32_t> counts(cat.activities.size(), 0);
        for (std::uint32_t i = 0; i < s.agents.size(); ++i) ++counts[s.agents.activity[i]];
        by_hour[static_cast<int>(std::floor(s.hour()))] = counts;
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) std::printf("  inconsistency: %s\n", why.c_str());
    CHECK(ok);
    std::printf("  simulated 24h with %zu residents in %.2fs (%.0f ticks/s)\n", s.agents.size(), secs, 86400.0 / secs);
    std::printf("  hour ");
    for (const auto& a : cat.activities) std::printf("%6.6s ", a.id.c_str());
    std::printf("\n");
    for (const auto& [h, counts] : by_hour) {
        std::printf("  %02d   ", h);
        for (auto c : counts) std::printf("%6u ", c);
        std::printf("\n");
    }
    auto pct = [&](int hour, int act) { return 100.0 * by_hour[hour][static_cast<std::size_t>(act)] / static_cast<double>(s.agents.size()); };
    CHECK(pct(3, cat.act_sleeping) > 60);                       // night: most people sleep
    CHECK(pct(11, cat.act_working) + pct(11, cat.act_studying) > 25); // late morning: work and school
    CHECK(pct(11, cat.act_sleeping) < 15);
    CHECK(pct(8, cat.act_travelling) > 1.5);                     // morning commute (short trips on the small map)
    // Variety: at least 10 different activities and 40 substates occurred during the day.
    std::set<int> acts, subs;
    for (std::uint32_t i = 0; i < s.agents.size(); ++i)
        for (int k = 0; k < pop::kHistory; ++k) {
            const auto& h = s.agents.history[i * pop::kHistory + static_cast<std::size_t>(k)];
            if (h.t >= 0) { acts.insert(h.activity); subs.insert(h.substate); }
        }
    std::printf("  activities seen: %zu, substates seen: %zu\n", acts.size(), subs.size());
    CHECK(acts.size() >= 10);
    CHECK(subs.size() >= 40);
    CHECK(sim.telemetry().samples().size() >= 1400);
}

TEST_CASE("sim/runs are deterministic and replays verify their checksum") {
    auto& e = env();
    REQUIRE(e.ok);
    sim::Simulation a(e.cat, e.lib), b(e.cat, e.lib);
    std::string err;
    REQUIRE(a.reset(small_config(21, 800), err));
    REQUIRE(b.reset(small_config(21, 800), err));
    a.run_ticks(1800);
    b.run_ticks(1800);
    json::Value cmd = json::Value::object();
    cmd.set("type", "inject_event");
    cmd.set("event", "fire");
    cmd.set("x", 300);
    cmd.set("y", 250);
    a.queue(cmd);
    b.queue(cmd);
    a.run_ticks(3600);
    b.run_ticks(3600);
    CHECK(a.checksum() == b.checksum());

    const json::Value saved = a.save_run();
    sim::Simulation c(e.cat, e.lib);
    bool same = false;
    REQUIRE(c.load_run(json::parse(saved.dump()).value, err, true, &same));
    CHECK(same);
    CHECK(c.checksum() == a.checksum());

    sim::Simulation d(e.cat, e.lib);
    REQUIRE(d.reset(small_config(22, 800), err));
    d.run_ticks(5400);
    CHECK(d.checksum() != a.checksum());
}

TEST_CASE("sim/events affect the city and residents react") {
    auto& e = env();
    REQUIRE(e.ok);
    sim::Simulation sim(e.cat, e.lib);
    std::string err;
    auto cfg = small_config(5, 2000);
    cfg.start_hour = 12.0;
    cfg.random_events = false;
    REQUIRE(sim.reset(cfg, err));
    sim.run_ticks(600);
    auto& s = sim.state();
    const Vec2 c{s.map.width / 2, s.map.height / 2};
    json::Value cmd = json::Value::object();
    cmd.set("type", "inject_event");
    cmd.set("event", "evacuation_alarm");
    cmd.set("x", c.x);
    cmd.set("y", c.y);
    cmd.set("radius", 150);
    cmd.set("duration", 30);
    REQUIRE(sim.apply(cmd, err));
    REQUIRE(s.events.size() == 1);
    sim.run_ticks(300);
    const auto& ev = s.events.front();
    std::size_t emergency = 0;
    for (std::uint32_t i = 0; i < s.agents.size(); ++i) emergency += s.agents.activity[i] == e.cat.act_emergency;
    std::printf("  alarm: perceived %u, reacted %u, in emergency now %zu\n", ev.perceived, ev.reacted, emergency);
    CHECK(ev.perceived > 20);
    CHECK(ev.reacted > 5);
    CHECK(emergency > 5);
    // Weather affects the whole city.
    cmd.set("event", "storm");
    cmd.set("type", "inject_event");
    REQUIRE(sim.apply(cmd, err));
    sim.run_ticks(10);
    CHECK(s.weather.kind == data::Weather::Storm);
    CHECK(s.weather.walk < 1.f);
    // After the alarm ends, everybody returns to normal life.
    sim.run_ticks(3600);
    emergency = 0;
    for (std::uint32_t i = 0; i < s.agents.size(); ++i) emergency += s.agents.activity[i] == e.cat.act_emergency;
    CHECK(emergency < s.agents.size() / 50 + 5);
    std::string why;
    const bool ok = consistent(sim, why);
    if (!ok) std::printf("  %s\n", why.c_str());
    CHECK(ok);
    CHECK(!s.event_history.empty() || !s.events.empty());
}

TEST_CASE("sim/random events appear with chaos and stop without it") {
    auto& e = env();
    REQUIRE(e.ok);
    sim::Simulation sim(e.cat, e.lib);
    std::string err;
    auto cfg = small_config(9, 1500);
    cfg.chaos = 6.f;
    REQUIRE(sim.reset(cfg, err));
    sim.run_ticks(6 * 3600);
    const auto created = sim.state().next_event_id - 1;
    std::printf("  chaos 6: %d events in 6 hours\n", created);
    CHECK(created >= 3);
    cfg.chaos = 0.f;
    REQUIRE(sim.reset(cfg, err));
    sim.run_ticks(3 * 3600);
    // Only emergent events (street fights escalating from encounters) can appear without chaos.
    int non_emergent = 0;
    for (const auto& ev : sim.state().event_history) non_emergent += e.cat.events[static_cast<std::size_t>(ev.type)].id != "street_dispute";
    for (const auto& ev : sim.state().events) non_emergent += e.cat.events[static_cast<std::size_t>(ev.type)].id != "street_dispute";
    CHECK(non_emergent == 0);
}

TEST_CASE("sim/scenario schedules events at their time") {
    auto& e = env();
    REQUIRE(e.ok);
    sim::Simulation sim(e.cat, e.lib);
    std::string err;
    const auto sc = json::parse(R"({
      "format": "npc-scenario", "name": "test",
      "config": {"seed": 3, "population": 600, "map_template": "old_town", "map_size": "small", "start_hour": 8, "random_events": false},
      "events": [{"at": "d0 08:30", "event": "concert"}, {"at": "d0 09:00", "event": "heavy_rain", "severity": 0.8}]
    })").value;
    REQUIRE(sim.load_scenario(sc, err));
    sim.run_ticks(29 * 60);
    CHECK(sim.state().next_event_id == 1);
    sim.run_ticks(2 * 60);
    CHECK(sim.state().next_event_id == 2);
    sim.run_ticks(30 * 60);
    CHECK(sim.state().next_event_id == 3);
    CHECK(sim.state().weather.kind == data::Weather::Rain);
    CHECK(!sim.load_scenario(json::parse(R"({"format":"npc-scenario","config":{},"events":[{"at":"bad","event":"fire"}]})").value, err));
    CHECK(!sim.load_scenario(json::parse(R"({"format":"npc-scenario","config":{},"events":[{"at":"08:00","event":"meteor"}]})").value, err));
}

TEST_CASE("sim/filters, inspector, frame and export") {
    auto& e = env();
    REQUIRE(e.ok);
    sim::Simulation sim(e.cat, e.lib);
    std::string err;
    REQUIRE(sim.reset(small_config(4, 1000), err));
    sim.run_ticks(3 * 3600);
    std::size_t all = 0, workers = 0, either = 0, old = 0;
    sim.query(json::parse(R"({"rules": []})").value, &all);
    sim.query(json::parse(R"({"logic":"all","rules":[{"field":"activity","value":"working"}]})").value, &workers);
    sim.query(json::parse(R"({"logic":"any","rules":[{"field":"activity","value":"working"},{"field":"activity","value":"sleeping"}]})").value, &either);
    sim.query(json::parse(R"({"rules":[{"field":"age","min":65}]})").value, &old);
    CHECK(all == 1000);
    CHECK(workers > 0 && workers < all);
    CHECK(either >= workers);
    CHECK(old > 0 && old < all);
    std::size_t stressed_retired = 0;
    sim.query(json::parse(R"({"rules":[{"field":"profile","value":"retired"},{"field":"need","name":"stress","max":0.5}]})").value, &stressed_retired);
    CHECK(stressed_retired <= old + 50);

    const auto j = sim.agent_json(10);
    CHECK(!j.str("name").empty());
    CHECK(j["traits"].size() == 12);
    CHECK(j["needs"].size() == 8);
    CHECK(j["agenda"].size() >= 1);
    CHECK(j["history"].size() >= 1);
    CHECK(j["relations"].size() >= 1);
    CHECK(j["explain"].size() >= 8);

    const auto frame = sim.frame_binary();
    CHECK(frame.size() == 4 + 4 + 8 + 4 + 1000 * 12 + 4 + sim.state().buses.size() * 12 + 4 + sim.state().map.edges.size());
    CHECK(frame[0] == 'N' && frame[3] == 'F');
    const auto csv = sim.export_csv(json::parse(R"({"rules":[{"field":"activity","value":"working"}]})").value);
    CHECK(static_cast<std::size_t>(std::count(csv.begin(), csv.end(), '\n')) == workers + 1);
    const auto st = sim.state_json();
    CHECK(st["activities"].size() == e.cat.activities.size());
    CHECK(st["substates"].size() == e.cat.substates.size());
}

TEST_CASE("sim/custom map file and larger templates run") {
    auto& e = env();
    REQUIRE(e.ok);
    sim::Simulation sim(e.cat, e.lib);
    std::string err;
    sim::Config cfg;
    cfg.map_file = std::string(NPC_DATA_DIR) + "/maps/custom/aldea.json";
    cfg.population = 60;
    const bool ok = sim.reset(cfg, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    REQUIRE(ok);
    sim.run_ticks(4 * 3600);
    std::string why;
    CHECK(consistent(sim, why));
    for (const char* tpl : {"river", "coastal", "suburban", "financial"}) {
        sim::Config c = small_config(12, 1200);
        c.map_template = tpl;
        REQUIRE(sim.reset(c, err));
        sim.run_ticks(2 * 3600);
        const bool good = consistent(sim, why);
        if (!good) std::printf("  %s: %s\n", tpl, why.c_str());
        CHECK(good);
    }
}
