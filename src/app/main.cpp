#include "simulation/simulation.hpp"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

using npc_crowd::Backend;
using npc_crowd::Config;

void print_usage(std::string_view exe) {
    std::cout << "Usage:\n"
              << "  " << exe << " --benchmark [--agents N] [--ticks N] [--seed N] [--speed X]\n"
              << "                 [--backend soa|oop] [--csv]\n"
              << "  " << exe << "            (no --benchmark: open the browser viewer)\n";
}

template <typename T>
bool parse_number(std::string_view text, T& out) {
    if constexpr (std::is_floating_point_v<T>) {
        try {
            std::size_t used = 0;
            out = static_cast<T>(std::stod(std::string(text), &used));
            return used == text.size();
        } catch (...) {
            return false;
        }
    } else {
        auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
        return ec == std::errc{} && ptr == text.data() + text.size();
    }
}

std::filesystem::path executable_dir(const char* argv0) {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return std::filesystem::path(buffer).parent_path();
#endif
    std::error_code ec;
    auto p = std::filesystem::absolute(argv0, ec);
    return ec ? std::filesystem::current_path() : p.parent_path();
}

int open_viewer(const char* argv0) {
    const auto exe_dir = executable_dir(argv0);
    std::filesystem::path index = exe_dir / "viewer" / "index.html";
    if (!std::filesystem::exists(index)) {
        // Fall back to the source tree layout (build/Release/../../viewer).
        for (auto dir = exe_dir; !dir.empty() && dir != dir.parent_path(); dir = dir.parent_path()) {
            if (std::filesystem::exists(dir / "viewer" / "index.html")) {
                index = dir / "viewer" / "index.html";
                break;
            }
        }
    }
    if (!std::filesystem::exists(index)) {
        std::cerr << "viewer/index.html not found next to the executable.\n";
        return 1;
    }
    std::cout << "Opening " << index.string() << '\n';
#ifdef _WIN32
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", index.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        std::cerr << "Could not open the viewer (ShellExecute error " << result << ").\n";
        return 1;
    }
    return 0;
#else
    std::cout << "Open that file in a browser.\n";
    return 0;
#endif
}

} // namespace

int main(int argc, char** argv) {
    const std::string_view exe = argc > 0 ? argv[0] : "npc_crowd_routines";
    Config config;
    bool benchmark = false;
    bool csv = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto next_value = [&](std::string_view name) -> std::string_view {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << '\n';
                std::exit(2);
            }
            return argv[++i];
        };
        auto bad_value = [&](std::string_view name, std::string_view value) {
            std::cerr << "Invalid value for " << name << ": " << value << '\n';
            std::exit(2);
        };

        if (arg == "--help" || arg == "-h") {
            print_usage(exe);
            return 0;
        } else if (arg == "--benchmark") {
            benchmark = true;
        } else if (arg == "--csv") {
            csv = true;
        } else if (arg == "--agents") {
            const auto v = next_value(arg);
            if (!parse_number(v, config.population) || config.population == 0) bad_value(arg, v);
        } else if (arg == "--ticks") {
            const auto v = next_value(arg);
            if (!parse_number(v, config.ticks) || config.ticks == 0) bad_value(arg, v);
        } else if (arg == "--seed") {
            const auto v = next_value(arg);
            if (!parse_number(v, config.seed)) bad_value(arg, v);
        } else if (arg == "--speed") {
            const auto v = next_value(arg);
            if (!parse_number(v, config.speed) || config.speed <= 0.f) bad_value(arg, v);
        } else if (arg == "--backend") {
            const auto v = next_value(arg);
            if (v == "soa" || v == "SoA" || v == "SOA") config.backend = Backend::Soa;
            else if (v == "oop" || v == "OOP") config.backend = Backend::Oop;
            else bad_value(arg, v);
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
            print_usage(exe);
            return 2;
        }
    }

    if (!benchmark) return open_viewer(argc > 0 ? argv[0] : "");

    npc_crowd::Simulation sim(config);
    const npc_crowd::Metrics m = sim.run();

    const auto backend = sim.backend_name();
    if (csv) {
        std::cout << "backend,agents,ticks,seed,speed,tick_p50_ms,tick_p95_ms,tick_p99_ms,elapsed_ms,"
                     "estimated_memory_bytes,events\n"
                  << backend << ',' << config.population << ',' << config.ticks << ',' << config.seed << ','
                  << config.speed << ',' << m.percentile(.50) << ',' << m.percentile(.95) << ','
                  << m.percentile(.99) << ',' << m.elapsed_ms << ',' << m.memory_bytes << ',' << m.events
                  << '\n';
    } else {
        std::cout << "backend                 " << backend << '\n'
                  << "agents                  " << config.population << '\n'
                  << "ticks                   " << config.ticks << '\n'
                  << "seed                    " << config.seed << '\n'
                  << "speed                   " << config.speed << '\n'
                  << "tick_p50_ms             " << m.percentile(.50) << '\n'
                  << "tick_p95_ms             " << m.percentile(.95) << '\n'
                  << "tick_p99_ms             " << m.percentile(.99) << '\n'
                  << "elapsed_ms              " << m.elapsed_ms << '\n'
                  << "estimated_memory_bytes  " << m.memory_bytes << '\n'
                  << "events                  " << m.events << '\n';
    }
    return 0;
}
