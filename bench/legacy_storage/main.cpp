// storage_benchmark: the original OOP vs SoA storage-layout micro-benchmark (see docs/decisions/ADR-0001).
// It is NOT the city simulation (that is npc_crowd_routines); it isolates the cost of the memory layout.
//
//   storage_benchmark --agents 5000 --ticks 1200 --seed 42 --backend soa|oop [--csv]

#include "simulation/simulation.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    npc_crowd::Config config;
    bool csv = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto value = [&]() -> std::string_view {
            if (i + 1 >= argc) { std::cerr << "missing value for " << arg << '\n'; std::exit(2); }
            return argv[++i];
        };
        auto number = [&](auto& out) {
            const auto v = value();
            auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
            if (ec != std::errc{} || p != v.data() + v.size()) { std::cerr << "invalid value for " << arg << '\n'; std::exit(2); }
        };
        if (arg == "--csv") csv = true;
        else if (arg == "--agents") number(config.population);
        else if (arg == "--ticks") number(config.ticks);
        else if (arg == "--seed") number(config.seed);
        else if (arg == "--speed") config.speed = std::strtof(std::string(value()).c_str(), nullptr);
        else if (arg == "--backend") config.backend = value() == "oop" ? npc_crowd::Backend::Oop : npc_crowd::Backend::Soa;
        else if (arg == "--benchmark") {}
        else { std::cerr << "usage: storage_benchmark --agents N --ticks N --seed N --backend soa|oop [--csv]\n"; return 2; }
    }
    npc_crowd::Simulation sim(config);
    const npc_crowd::Metrics m = sim.run();
    if (csv) {
        std::cout << "backend,agents,ticks,seed,tick_p50_ms,tick_p95_ms,tick_p99_ms,elapsed_ms,estimated_memory_bytes\n"
                  << sim.backend_name() << ',' << config.population << ',' << config.ticks << ',' << config.seed << ','
                  << m.percentile(.5) << ',' << m.percentile(.95) << ',' << m.percentile(.99) << ',' << m.elapsed_ms << ',' << m.memory_bytes << '\n';
    } else {
        std::cout << "backend " << sim.backend_name() << "\nagents " << config.population << "\nticks " << config.ticks
                  << "\ntick_p50_ms " << m.percentile(.5) << "\ntick_p95_ms " << m.percentile(.95) << "\ntick_p99_ms " << m.percentile(.99)
                  << "\nelapsed_ms " << m.elapsed_ms << "\nestimated_memory_bytes " << m.memory_bytes << '\n';
    }
    return 0;
}
