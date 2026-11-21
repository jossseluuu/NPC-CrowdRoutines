#include "server/api.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace npc::server {

namespace fs = std::filesystem;

namespace {

std::string safe_name(std::string s) {
    for (auto& c : s)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) c = '_';
    if (s.empty()) s = "untitled";
    return s.substr(0, 80);
}

json::Value list_json_files(const std::string& dir, const std::string& format) {
    json::Value out = json::Value::array();
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir, ec))
        if (entry.path().extension() == ".json") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    for (const auto& p : files) {
        auto r = json::parse_file(p.string());
        if (!r.ok() || r.value.str("format") != format) continue;
        json::Value j = json::Value::object();
        j.set("file", p.filename().string());
        j.set("name", r.value.str("name", p.stem().string()));
        if (r.value.contains("description")) j.set("description", r.value.str("description"));
        if (format == "npc-run") { j.set("end_time", r.value.str("end_time")); j.set("scenario", r.value.str("scenario")); }
        out.push_back(std::move(j));
    }
    return out;
}

json::Value parse_body(const Request& req, Response& err, bool& ok) {
    auto r = json::parse(req.body.empty() ? "{}" : req.body);
    ok = r.ok();
    if (!ok) err = Response::error(400, "invalid JSON body: " + r.error);
    return r.value;
}

json::Value status_json(const RunnerStatus& st) {
    json::Value v = json::Value::object();
    v.set("running", st.running);
    v.set("speed", st.speed);
    v.set("achieved", std::round(st.achieved * 10) / 10);
    v.set("tick_ms_p50", std::round(st.tick_ms_p50 * 1000) / 1000);
    v.set("tick_ms_p95", std::round(st.tick_ms_p95 * 1000) / 1000);
    v.set("tick_ms_p99", std::round(st.tick_ms_p99 * 1000) / 1000);
    v.set("tick_ms_max", std::round(st.tick_ms_max * 1000) / 1000);
    v.set("ticks", static_cast<double>(st.ticks));
    return v;
}

} // namespace

void register_api(HttpServer& http, SimRunner& runner, const AppPaths& paths, const data::Catalog& cat, const world::MapLibrary& maps) {
    const std::string scenarios_dir = paths.data_dir + "/scenarios";
    const std::string custom_maps_dir = paths.data_dir + "/maps/custom";

    http.route("GET", "/api/info", [&, scenarios_dir, custom_maps_dir](const Request&) {
        json::Value v = runner.with_sim([](sim::Simulation& s) { return s.info_json(); });
        json::Value tpls = json::Value::array(), sizes = json::Value::array(), langs = json::Value::array();
        for (const auto& t : maps.templates) tpls.push_back(t.id);
        for (const auto& s : maps.sizes) {
            json::Value j = json::Value::object();
            j.set("id", s.id);
            j.set("width", s.width);
            j.set("height", s.height);
            j.set("population", s.population);
            sizes.push_back(std::move(j));
        }
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(paths.data_dir + "/i18n", ec))
            if (e.path().extension() == ".json") langs.push_back(e.path().stem().string());
        v.set("templates", std::move(tpls));
        v.set("sizes", std::move(sizes));
        v.set("languages", std::move(langs));
        v.set("scenarios", list_json_files(scenarios_dir, "npc-scenario"));
        v.set("custom_maps", list_json_files(custom_maps_dir, "npc-city-map"));
        v.set("runs", list_json_files(paths.runs_dir, "npc-run"));
        v.set("runner", status_json(runner.status()));
        return Response::json(v.dump());
    });

    http.route("GET", "/api/catalog", [&](const Request&) { return Response::json(cat.to_json().dump()); });

    http.route("GET", "/api/i18n", [&](const Request& req) {
        const std::string lang = safe_name(req.param("lang", "es"));
        std::ifstream in(paths.data_dir + "/i18n/" + lang + ".json", std::ios::binary);
        if (!in) return Response::error(404, "language not available: " + lang);
        std::ostringstream ss;
        ss << in.rdbuf();
        return Response::json(ss.str());
    });

    http.route("GET", "/api/map", [&](const Request&) { return Response::json(runner.map()); });

    http.route("GET", "/api/residents", [&](const Request&) {
        return Response::json(runner.with_sim([](sim::Simulation& s) { return s.residents_json(); }).dump());
    });

    http.route("GET", "/api/frame", [&](const Request&) {
        const auto f = runner.frame();
        Response r;
        r.content_type = "application/octet-stream";
        r.body.assign(f.begin(), f.end());
        return r;
    });

    http.route("GET", "/api/state", [&](const Request&) {
        auto r = json::parse(runner.state());
        json::Value v = r.ok() ? r.value : json::Value::object();
        v.set("runner", status_json(runner.status()));
        return Response::json(v.dump());
    });

    http.route("GET", "/api/agent", [&](const Request& req) {
        const auto id = std::strtoul(req.param("id", "0").c_str(), nullptr, 10);
        auto v = runner.with_sim([&](sim::Simulation& s) { return s.agent_json(static_cast<std::uint32_t>(id)); });
        if (v.is_null()) return Response::error(404, "no resident with id " + std::to_string(id));
        return Response::json(v.dump());
    });

    http.route("GET", "/api/building", [&](const Request& req) {
        const auto id = static_cast<std::uint32_t>(std::strtoul(req.param("id", "0").c_str(), nullptr, 10));
        auto v = runner.with_sim([&](sim::Simulation& sim) {
            const auto& s = sim.state();
            if (id >= s.map.buildings.size()) return json::Value();
            const auto& b = s.map.buildings[id];
            const auto& bt = cat.building_types[static_cast<std::size_t>(b.type)];
            json::Value j = json::Value::object();
            j.set("id", id);
            j.set("type", bt.id);
            j.set("category", cat.categories[static_cast<std::size_t>(b.category)]);
            j.set("district", s.map.districts[static_cast<std::size_t>(b.district)].name);
            j.set("floors", b.floors);
            j.set("households", b.households);
            j.set("jobs", b.jobs);
            j.set("students", b.students);
            j.set("occupancy", s.occupancy[id]);
            j.set("open", bt.is_open(s.clock.weekday(), s.hour()) && s.building_open(id));
            j.set("closed_by_event", !s.building_open(id));
            json::Value inside = json::Value::array();
            std::size_t residents = 0, workers = 0;
            for (std::uint32_t i = 0; i < s.agents.size(); ++i) {
                residents += s.agents.home[i] == id;
                workers += s.agents.work[i] == id && s.agents.home[i] != id;
                if (s.agents.location[i] == id && inside.size() < 40) {
                    json::Value p = json::Value::object();
                    p.set("id", i);
                    p.set("name", sim.resident_name(i));
                    p.set("substate", cat.substates[s.agents.substate[i]].id);
                    inside.push_back(std::move(p));
                }
            }
            j.set("residents", static_cast<double>(residents));
            j.set("workers", static_cast<double>(workers));
            j.set("inside", std::move(inside));
            return j;
        });
        if (v.is_null()) return Response::error(404, "no such building");
        return Response::json(v.dump());
    });

    http.route("POST", "/api/query", [&](const Request& req) {
        Response err;
        bool ok;
        const auto filter = parse_body(req, err, ok);
        if (!ok) return err;
        std::size_t matches = 0;
        const auto bits = runner.with_sim([&](sim::Simulation& s) { return s.query(filter, &matches); });
        Response r;
        r.content_type = "application/octet-stream";
        r.body.assign(bits.begin(), bits.end());
        r.headers["X-Matches"] = std::to_string(matches);
        r.headers["Access-Control-Expose-Headers"] = "X-Matches";
        return r;
    });

    http.route("POST", "/api/export", [&](const Request& req) {
        Response err;
        bool ok;
        const auto filter = parse_body(req, err, ok);
        if (!ok) return err;
        Response r;
        r.content_type = "text/csv; charset=utf-8";
        r.body = runner.with_sim([&](sim::Simulation& s) { return s.export_csv(filter); });
        r.headers["Content-Disposition"] = "attachment; filename=\"residents.csv\"";
        return r;
    });

    http.route("GET", "/api/timeseries", [&](const Request& req) {
        const double since = std::strtod(req.param("since", "-1").c_str(), nullptr);
        const auto max = std::strtoul(req.param("max", "720").c_str(), nullptr, 10);
        return Response::json(runner.with_sim([&](sim::Simulation& s) { return s.telemetry().series(s.state(), since, max); }).dump());
    });

    http.route("GET", "/api/telemetry.csv", [&](const Request&) {
        const std::string tmp = paths.runs_dir + "/.telemetry_export.csv";
        std::error_code ec;
        fs::create_directories(paths.runs_dir, ec);
        runner.with_sim([&](sim::Simulation& s) { return s.telemetry().write_csv(s.state(), tmp); });
        std::ifstream in(tmp, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        Response r;
        r.content_type = "text/csv; charset=utf-8";
        r.body = ss.str();
        r.headers["Content-Disposition"] = "attachment; filename=\"telemetry.csv\"";
        return r;
    });

    http.route("POST", "/api/control", [&](const Request& req) {
        Response err;
        bool ok;
        const auto c = parse_body(req, err, ok);
        if (!ok) return err;
        const std::string action = c.str("action");
        if (action == "pause") runner.set_running(false);
        else if (action == "resume") runner.set_running(true);
        else if (action == "toggle") runner.set_running(!runner.status().running);
        else if (action == "step") runner.step(static_cast<std::uint64_t>(std::max(1LL, c.integer("ticks", 1))));
        else if (action == "speed") runner.set_speed(c.num("value", 60));
        else return Response::error(400, "unknown control action '" + action + "'");
        return Response::json(status_json(runner.status()).dump());
    });

    http.route("POST", "/api/command", [&](const Request& req) {
        Response err;
        bool ok;
        const auto c = parse_body(req, err, ok);
        if (!ok) return err;
        // Validate synchronously so the client gets immediate feedback, then queue for tick-exact application.
        const std::string type = c.str("type");
        if (type == "inject_event" && cat.event_index(c.str("event")) < 0) return Response::error(400, "unknown event '" + c.str("event") + "'");
        if (type != "inject_event" && type != "end_event" && type != "set_chaos" && type != "set_random_events")
            return Response::error(400, "unknown command '" + type + "'");
        runner.with_sim([&](sim::Simulation& s) { s.queue(c); return 0; });
        if (!runner.status().running) runner.step(1); // apply now while paused
        return Response::json("{\"ok\": true}");
    });

    http.route("POST", "/api/reset", [&, custom_maps_dir](const Request& req) {
        Response err;
        bool ok;
        const auto body = parse_body(req, err, ok);
        if (!ok) return err;
        sim::Config cfg = sim::Config::from_json(body);
        if (!cfg.map_file.empty()) cfg.map_file = custom_maps_dir + "/" + fs::path(cfg.map_file).filename().string();
        std::string error;
        const bool done = runner.rebuild([&](sim::Simulation& s, std::string& e) { return s.reset(cfg, e); }, error);
        if (!done) return Response::error(400, error);
        return Response::json(runner.with_sim([](sim::Simulation& s) { return s.info_json(); }).dump());
    });

    http.route("POST", "/api/scenario/load", [&, scenarios_dir](const Request& req) {
        Response err;
        bool ok;
        const auto body = parse_body(req, err, ok);
        if (!ok) return err;
        json::Value scenario = body["scenario"];
        if (scenario.is_null()) {
            auto r = json::parse_file(scenarios_dir + "/" + fs::path(body.str("file")).filename().string());
            if (!r.ok()) return Response::error(404, r.error);
            scenario = r.value;
        }
        std::string error;
        const bool done = runner.rebuild([&](sim::Simulation& s, std::string& e) { return s.load_scenario(scenario, e); }, error);
        if (!done) return Response::error(400, error);
        return Response::json(runner.with_sim([](sim::Simulation& s) { return s.info_json(); }).dump());
    });

    http.route("POST", "/api/scenario/save", [&, scenarios_dir](const Request& req) {
        // Saves the current configuration + the events injected so far as a reusable scenario.
        Response err;
        bool ok;
        const auto body = parse_body(req, err, ok);
        if (!ok) return err;
        const std::string name = body.str("name", "mi_escenario");
        json::Value sc = runner.with_sim([&](sim::Simulation& s) {
            json::Value v = json::Value::object();
            v.set("format", "npc-scenario");
            v.set("version", 1);
            v.set("name", name);
            v.set("description", body.str("description", ""));
            v.set("config", s.state().config.to_json());
            json::Value evs = json::Value::array();
            for (const auto& c : s.command_log()) {
                if (c.body.str("type") != "inject_event") continue;
                json::Value e = c.body;
                e.as_object_mut().erase(std::remove_if(e.as_object_mut().begin(), e.as_object_mut().end(),
                                                       [](const auto& kv) { return kv.first == "type" || kv.first == "at"; }),
                                        e.as_object_mut().end());
                SimClock clk = s.state().clock;
                clk.tick = c.tick;
                json::Value full = json::Value::object();
                full.set("at", clk.label());
                for (const auto& [k, val] : e.as_object()) full.set(k, val);
                evs.push_back(std::move(full));
            }
            v.set("events", std::move(evs));
            return v;
        });
        std::error_code ec;
        fs::create_directories(scenarios_dir, ec);
        const std::string file = safe_name(name) + ".json";
        if (!json::write_file(scenarios_dir + "/" + file, sc)) return Response::error(500, "cannot write scenario file");
        json::Value res = json::Value::object();
        res.set("file", file);
        res.set("scenario", sc);
        return Response::json(res.dump());
    });

    http.route("POST", "/api/run/save", [&](const Request& req) {
        Response err;
        bool ok;
        const auto body = parse_body(req, err, ok);
        if (!ok) return err;
        json::Value run = runner.with_sim([](sim::Simulation& s) { return s.save_run(); });
        run.set("name", body.str("name", "run"));
        std::error_code ec;
        fs::create_directories(paths.runs_dir, ec);
        const std::string file = safe_name(body.str("name", "run")) + ".json";
        if (!json::write_file(paths.runs_dir + "/" + file, run)) return Response::error(500, "cannot write run file");
        json::Value res = json::Value::object();
        res.set("file", file);
        res.set("run", run);
        return Response::json(res.dump());
    });

    http.route("POST", "/api/run/load", [&](const Request& req) {
        Response err;
        bool ok;
        const auto body = parse_body(req, err, ok);
        if (!ok) return err;
        json::Value run = body["run"];
        if (run.is_null()) {
            auto r = json::parse_file(paths.runs_dir + "/" + fs::path(body.str("file")).filename().string());
            if (!r.ok()) return Response::error(404, r.error);
            run = r.value;
        }
        const bool replay = body.boolean("replay", false);
        bool same = false;
        std::string error;
        runner.set_running(false);
        const bool done = runner.rebuild([&](sim::Simulation& s, std::string& e) { return s.load_run(run, e, replay, &same); }, error);
        if (!done) return Response::error(400, error);
        json::Value res = runner.with_sim([](sim::Simulation& s) { return s.info_json(); });
        if (replay) res.set("checksum_ok", same);
        return Response::json(res.dump());
    });

    http.route("POST", "/api/map/upload", [&, custom_maps_dir](const Request& req) {
        Response err;
        bool ok;
        const auto body = parse_body(req, err, ok);
        if (!ok) return err;
        const json::Value& m = body["map"].is_null() ? body : body["map"];
        world::CityMap test;
        std::string error;
        if (!world::CityMap::from_json(m, cat, test, error) || !test.finalize(cat, error)) return Response::error(400, error);
        std::error_code ec;
        fs::create_directories(custom_maps_dir, ec);
        const std::string file = safe_name(m.str("name", "custom")) + ".json";
        if (!json::write_file(custom_maps_dir + "/" + file, m)) return Response::error(500, "cannot write map file");
        json::Value res = json::Value::object();
        res.set("file", file);
        res.set("buildings", static_cast<double>(test.buildings.size()));
        res.set("households", static_cast<double>(test.total_households()));
        return Response::json(res.dump());
    });

    http.route("GET", "/api/map/export", [&](const Request&) {
        Response r = Response::json(runner.map());
        r.headers["Content-Disposition"] = "attachment; filename=\"city_map.json\"";
        return r;
    });

    http.static_dir("/", paths.web_dir);
}

} // namespace npc::server
