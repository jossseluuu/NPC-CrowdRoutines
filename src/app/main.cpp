// npc_crowd_routines: command line entry point of the C++ simulation engine.
//
//   serve          run the simulation with the web interface (http://localhost:8080)
//   run            headless run, writes telemetry CSV, summary JSON and a replayable run file
//   replay         re-simulates a saved run and verifies its checksum
//   benchmark      measures tick times for several populations and writes JSON/Markdown reports
//   generate-map   generates a city from a template and writes it as an editable JSON map
//   validate-map   checks a custom map file and prints a summary
//
// Run without arguments or with --help for the full option list.

#include "data/catalog.hpp"
#include "server/api.hpp"
#include "server/http.hpp"
#include "server/runner.hpp"
#include "sim/simulation.hpp"
#include "world/mapgen.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace npc;
namespace fs = std::filesystem;

namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

struct Args {
    std::string command;
    std::map<std::string, std::string> opts;
    bool has(const std::string& k) const { return opts.count(k) > 0; }
    std::string get(const std::string& k, const std::string& def = "") const {
        const auto it = opts.find(k);
        return it == opts.end() ? def : it->second;
    }
    double num(const std::string& k, double def) const { return has(k) ? std::strtod(get(k).c_str(), nullptr) : def; }
};

Args parse_args(int argc, char** argv) {
    Args a;
    int i = 1;
    if (argc > 1 && argv[1][0] != '-') { a.command = argv[1]; i = 2; }
    for (; i < argc; ++i) {
        std::string k = argv[i];
        if (k.rfind("--", 0) != 0) { a.opts["file"] = k; continue; }
        k = k.substr(2);
        const auto eq = k.find('=');
        if (eq != std::string::npos) { a.opts[k.substr(0, eq)] = k.substr(eq + 1); continue; }
        if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) a.opts[k] = argv[++i];
        else a.opts[k] = "1";
    }
    return a;
}

void usage() {
    std::puts(R"(npc_crowd_routines - NPC routines & crowd stress-test sandbox (C++ engine)

Usage: npc_crowd_routines <command> [options]

Commands:
  serve            Start the simulation server and web interface.
                   --port 8080  --host 127.0.0.1  --web web  --data data  --runs runs  --paused
                   --scenario data/scenarios/rush_hour.json  (or the config options below)
  run              Headless run.  --hours 24  --out runs/<name>  plus config options
                   or --scenario file.json
  replay <file>    Re-simulate a saved run and verify the checksum.
  benchmark        --agents 2500,10000,25000  --ticks 1800  --warmup 600  --template grid
                   --size large  --out reports/benchmark.json  --markdown reports/benchmark.md
  generate-map     --template river --size medium --seed 7 --out my_map.json [--name "Mi ciudad"]
  validate-map <file>

Config options (serve/run/benchmark):
  --seed 42  --population 3000  --template grid|old_town|river|suburban|financial|coastal
  --size small|medium|large  --map-file data/maps/custom/aldea.json  --start-hour 7
  --weekday 0..6  --chaos 1.0  --no-random-events  --step 1.0

Run everything from the repository root (paths default to ./data, ./web, ./runs).
)");
}

sim::Config config_from(const Args& a) {
    sim::Config c;
    c.seed = static_cast<std::uint64_t>(a.num("seed", 42));
    c.population = static_cast<int>(a.num("population", 0));
    c.map_template = a.get("template", "grid");
    c.map_size = a.get("size", "small");
    c.map_seed = static_cast<std::uint64_t>(a.num("map-seed", 0));
    c.map_file = a.get("map-file", "");
    c.start_hour = a.num("start-hour", 7);
    c.start_weekday = static_cast<int>(a.num("weekday", 0));
    c.chaos = static_cast<float>(a.num("chaos", 1));
    c.random_events = !a.has("no-random-events");
    c.step_seconds = a.num("step", 1.0);
    c.name = a.get("name", "Sandbox");
    return c;
}

bool load_catalog(const Args& a, data::Catalog& cat, world::MapLibrary& lib) {
    const std::string dir = a.get("data", "data");
    std::string err;
    if (!cat.load(dir, err) || !lib.load(dir, cat, err)) {
        std::fprintf(stderr, "error loading data from '%s': %s\n", dir.c_str(), err.c_str());
        std::fprintf(stderr, "hint: run from the repository root or pass --data <path-to-data>\n");
        return false;
    }
    return true;
}

bool setup_sim(const Args& a, sim::Simulation& sim) {
    std::string err;
    bool ok;
    if (a.has("scenario")) {
        auto r = json::parse_file(a.get("scenario"));
        ok = r.ok() && sim.load_scenario(r.value, err);
        if (!r.ok()) err = r.error;
    } else {
        ok = sim.reset(config_from(a), err);
    }
    if (!ok) std::fprintf(stderr, "error: %s\n", err.c_str());
    return ok;
}

std::string compiler_info() {
#if defined(__clang__)
    return "clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
    return "gcc " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(_MSC_VER)
    return "msvc " + std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
}

// ---------------------------------------------------------------------------------------------
int cmd_serve(const Args& a) {
    data::Catalog cat;
    world::MapLibrary lib;
    if (!load_catalog(a, cat, lib)) return 1;
    server::SimRunner runner(cat, lib);
    std::string err;
    const bool built = runner.rebuild([&](sim::Simulation& s, std::string& e) {
        if (a.has("scenario")) {
            auto r = json::parse_file(a.get("scenario"));
            if (!r.ok()) { e = r.error; return false; }
            return s.load_scenario(r.value, e);
        }
        return s.reset(config_from(a), e);
    }, err);
    if (!built) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    runner.set_speed(a.num("speed", 60));
    runner.set_running(!a.has("paused"));
    runner.start();

    server::HttpServer http;
    server::AppPaths paths{a.get("data", "data"), a.get("web", "web"), a.get("runs", "runs")};
    server::register_api(http, runner, paths, cat, lib);
    const std::string host = a.get("host", "127.0.0.1");
    if (!http.start(host, static_cast<int>(a.num("port", 8080)), err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    const auto info = runner.with_sim([](sim::Simulation& s) { return s.info_json(); });
    std::printf("NPC crowd routines - %s, %d residents on '%s'\n", info["config"].str("name").c_str(),
                static_cast<int>(info.integer("population", 0)), info["map"].str("name").c_str());
    std::printf("Open http://%s:%d in your browser (Ctrl+C to stop)\n", host == "0.0.0.0" ? "localhost" : host.c_str(), http.port());
    std::fflush(stdout);
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    while (!g_stop) std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::puts("\nstopping...");
    http.stop();
    runner.stop();
    return 0;
}

int cmd_run(const Args& a) {
    data::Catalog cat;
    world::MapLibrary lib;
    if (!load_catalog(a, cat, lib)) return 1;
    sim::Simulation sim(cat, lib);
    if (!setup_sim(a, sim)) return 1;
    const double hours = a.num("hours", 24);
    const std::string out = a.get("out", "runs/headless");
    std::error_code ec;
    fs::create_directories(out, ec);
    const auto ticks = static_cast<std::uint64_t>(hours * 3600.0 / sim.state().clock.step_seconds);
    std::printf("running %.1f simulated hours with %zu residents...\n", hours, sim.state().agents.size());
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t k = 0; k < ticks && !g_stop; ++k) {
        sim.step();
        if (k % 36000 == 0 && k) std::printf("  %s\n", sim.state().clock.label().c_str());
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    sim.telemetry().write_csv(sim.state(), out + "/telemetry.csv");
    json::write_file(out + "/run.json", sim.save_run());
    json::Value summary = sim.state_json(50);
    summary.set("wall_seconds", secs);
    summary.set("speedup", hours * 3600.0 / secs);
    summary.set("info", sim.info_json());
    json::write_file(out + "/summary.json", summary);
    std::printf("done in %.1fs (x%.0f real time). Output: %s/{telemetry.csv,summary.json,run.json}\n", secs, hours * 3600.0 / secs, out.c_str());
    return 0;
}

int cmd_replay(const Args& a) {
    data::Catalog cat;
    world::MapLibrary lib;
    if (!load_catalog(a, cat, lib)) return 1;
    const std::string file = a.get("file", a.get("run"));
    auto r = json::parse_file(file);
    if (!r.ok()) { std::fprintf(stderr, "error: %s\n", r.error.c_str()); return 1; }
    sim::Simulation sim(cat, lib);
    std::string err;
    bool same = false;
    const auto t0 = std::chrono::steady_clock::now();
    if (!sim.load_run(r.value, err, true, &same)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("replayed to %s in %.1fs: checksum %s\n", sim.state().clock.label().c_str(), secs, same ? "MATCHES (identical run)" : "DIFFERS");
    return same ? 0 : 2;
}

int cmd_benchmark(const Args& a) {
    data::Catalog cat;
    world::MapLibrary lib;
    if (!load_catalog(a, cat, lib)) return 1;
    std::vector<int> pops;
    std::stringstream ss(a.get("agents", "2500,10000,25000"));
    for (std::string tok; std::getline(ss, tok, ',');) if (!tok.empty()) pops.push_back(std::stoi(tok));
    const auto ticks = static_cast<std::uint64_t>(a.num("ticks", 1800));
    const auto warmup = static_cast<std::uint64_t>(a.num("warmup", 600));
    json::Value report = json::Value::object();
    report.set("format", "npc-benchmark");
    report.set("version", 1);
    report.set("compiler", compiler_info());
#ifdef NDEBUG
    report.set("build", "release");
#else
    report.set("build", "debug");
#endif
    report.set("threads_available", static_cast<int>(std::thread::hardware_concurrency()));
    report.set("template", a.get("template", "grid"));
    report.set("size", a.get("size", "large"));
    report.set("ticks", static_cast<double>(ticks));
    report.set("warmup", static_cast<double>(warmup));
    report.set("start_hour", a.num("start-hour", 7));
    json::Value rows = json::Value::array();
    for (int p : pops) {
        Args b = a;
        b.opts["population"] = std::to_string(p);
        if (!b.has("size")) b.opts["size"] = "large";
        sim::Simulation sim(cat, lib);
        if (!setup_sim(b, sim)) return 1;
        sim.run_ticks(warmup);
        std::vector<double> ms;
        ms.reserve(ticks);
        const auto t0 = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < ticks; ++k) {
            const auto s0 = std::chrono::steady_clock::now();
            sim.step();
            ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count());
        }
        const double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::vector<double> sorted = ms;
        std::sort(sorted.begin(), sorted.end());
        auto pct = [&](double q) { return sorted[std::min(sorted.size() - 1, static_cast<std::size_t>(q * static_cast<double>(sorted.size() - 1)))]; };
        double mean = 0;
        for (double v : ms) mean += v;
        mean /= static_cast<double>(ms.size());
        const auto& s = sim.state();
        // Memory estimate: per-resident arrays + ring buffers + relationships.
        const double bytes = static_cast<double>(s.agents.size()) *
                                 (sizeof(data::NeedArray) + sizeof(data::TraitArray) + sizeof(pop::MoveState) + 128 +
                                  pop::kHistory * sizeof(pop::HistoryEntry) + pop::kActions * sizeof(pop::ActionEntry)) +
                             static_cast<double>(s.agents.rels.size() * sizeof(pop::Relation));
        json::Value row = json::Value::object();
        row.set("population", static_cast<double>(s.agents.size()));
        row.set("map", s.map.name);
        row.set("buildings", static_cast<double>(s.map.buildings.size()));
        row.set("tick_ms_mean", std::round(mean * 1000) / 1000);
        row.set("tick_ms_p50", std::round(pct(0.5) * 1000) / 1000);
        row.set("tick_ms_p95", std::round(pct(0.95) * 1000) / 1000);
        row.set("tick_ms_p99", std::round(pct(0.99) * 1000) / 1000);
        row.set("tick_ms_max", std::round(sorted.back() * 1000) / 1000);
        row.set("max_speedup", std::round(static_cast<double>(ticks) * s.clock.step_seconds / total));
        row.set("memory_mb", std::round(bytes / 1e6 * 10) / 10);
        row.set("events_created", s.next_event_id - 1);
        rows.push_back(row);
        std::printf("%6d residents: p50 %.3f ms  p95 %.3f ms  p99 %.3f ms  max x%.0f real time  ~%.1f MB\n", p, pct(0.5), pct(0.95), pct(0.99),
                    static_cast<double>(ticks) * s.clock.step_seconds / total, bytes / 1e6);
    }
    report.set("results", std::move(rows));
    const std::string out = a.get("out", "");
    if (!out.empty()) {
        std::error_code ec;
        fs::create_directories(fs::path(out).parent_path(), ec);
        json::write_file(out, report);
        std::printf("JSON report: %s\n", out.c_str());
    }
    const std::string md = a.get("markdown", "");
    if (!md.empty()) {
        std::error_code ec;
        fs::create_directories(fs::path(md).parent_path(), ec);
        std::ofstream f(md);
        f << "# Informe de rendimiento\n\n";
        f << "| Parámetro | Valor |\n|---|---|\n";
        f << "| Compilador | " << report.str("compiler") << " (" << report.str("build") << ") |\n";
        f << "| Hilos disponibles | " << report.integer("threads_available", 0) << " (la simulación usa 1) |\n";
        f << "| Mapa | " << report.str("template") << " / " << report.str("size") << " |\n";
        f << "| Ticks medidos | " << ticks << " (tras " << warmup << " de calentamiento), 1 tick = 1 s simulado |\n\n";
        f << "| Residentes | p50 (ms) | p95 (ms) | p99 (ms) | máx (ms) | Velocidad máx. | Memoria aprox. |\n|---:|---:|---:|---:|---:|---:|---:|\n";
        for (const auto& r : report["results"].as_array())
            f << "| " << r.integer("population", 0) << " | " << r.num("tick_ms_p50", 0) << " | " << r.num("tick_ms_p95", 0) << " | "
              << r.num("tick_ms_p99", 0) << " | " << r.num("tick_ms_max", 0) << " | ×" << r.integer("max_speedup", 0) << " | "
              << r.num("memory_mb", 0) << " MB |\n";
        f << "\n*Velocidad máx.*: segundos simulados por segundo real que el motor puede sostener en esta máquina.\n";
        std::printf("Markdown report: %s\n", md.c_str());
    }
    return 0;
}

int cmd_generate_map(const Args& a) {
    data::Catalog cat;
    world::MapLibrary lib;
    if (!load_catalog(a, cat, lib)) return 1;
    world::CityMap m;
    std::string err;
    const world::MapSpec spec{a.get("template", "grid"), a.get("size", "small"), static_cast<std::uint64_t>(a.num("seed", 42)), a.get("name", "")};
    if (!world::generate_map(spec, lib, cat, m, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    const std::string out = a.get("out", "city_map.json");
    if (!json::write_file(out, m.to_json(cat))) { std::fprintf(stderr, "error: cannot write %s\n", out.c_str()); return 1; }
    std::printf("%s: %zu intersections, %zu streets, %zu buildings, %zu homes, %zu jobs, %zu bus lines -> %s\n", m.name.c_str(),
                m.nodes.size(), m.edges.size(), m.buildings.size(), m.total_households(), m.total_jobs(), m.bus_lines.size(), out.c_str());
    return 0;
}

int cmd_validate_map(const Args& a) {
    data::Catalog cat;
    world::MapLibrary lib;
    if (!load_catalog(a, cat, lib)) return 1;
    const std::string file = a.get("file");
    auto r = json::parse_file(file);
    if (!r.ok()) { std::fprintf(stderr, "invalid JSON: %s\n", r.error.c_str()); return 1; }
    world::CityMap m;
    std::string err;
    if (!world::CityMap::from_json(r.value, cat, m, err) || !m.finalize(cat, err)) { std::fprintf(stderr, "invalid map: %s\n", err.c_str()); return 1; }
    std::printf("OK '%s': %zu intersections, %zu streets, %zu buildings, %zu homes (~%zu residents), %zu jobs, %zu bus lines\n",
                m.name.c_str(), m.nodes.size(), m.edges.size(), m.buildings.size(), m.total_households(), m.total_households() * 2,
                m.total_jobs(), m.bus_lines.size());
    std::map<std::string, int> missing;
    for (const auto& [type, n] : cat.minimums)
        if (m.buildings_by_type[static_cast<std::size_t>(type)].empty()) missing[cat.building_types[static_cast<std::size_t>(type)].id] = n;
    for (const auto& [t, n] : missing) std::printf("  warning: no '%s' (residents needing it will look elsewhere or skip it)\n", t.c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const Args a = parse_args(argc, argv);
    if (a.command.empty() || a.has("help") || a.command == "help") { usage(); return a.command.empty() && argc > 1 ? 1 : 0; }
    std::signal(SIGINT, on_signal);
    if (a.command == "serve") return cmd_serve(a);
    if (a.command == "run") return cmd_run(a);
    if (a.command == "replay") return cmd_replay(a);
    if (a.command == "benchmark") return cmd_benchmark(a);
    if (a.command == "generate-map") return cmd_generate_map(a);
    if (a.command == "validate-map") return cmd_validate_map(a);
    std::fprintf(stderr, "unknown command '%s'\n\n", a.command.c_str());
    usage();
    return 1;
}
