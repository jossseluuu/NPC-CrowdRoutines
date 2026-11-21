#pragma once
// Simulation coordinator: fixed-step loop, commands, scenarios, save/replay and client-facing queries.

#include "sim/state.hpp"
#include "sim/telemetry.hpp"
#include "world/mapgen.hpp"

#include <deque>
#include <string>
#include <vector>

namespace npc::sim {

struct Command {
    std::uint64_t tick = 0;
    json::Value body;
};

class Simulation {
public:
    Simulation(const data::Catalog& catalog, const world::MapLibrary& maps);

    /// Builds the map, the population and the transport system from a configuration.
    bool reset(const Config& config, std::string& error);
    /// Loads a scenario file (format "npc-scenario"): configuration plus scheduled events.
    bool load_scenario(const json::Value& scenario, std::string& error);
    /// Saved run (format "npc-run"): configuration + every command with its tick + final checksum.
    json::Value save_run() const;
    /// Restores a saved run. With replay=true it re-simulates up to the saved end tick and verifies the checksum.
    bool load_run(const json::Value& run, std::string& error, bool replay, bool* checksum_ok = nullptr);

    /// Advances one fixed step.
    void step();
    void run_ticks(std::uint64_t n) { for (std::uint64_t k = 0; k < n; ++k) step(); }

    /// Queues a command to be applied at the start of the next step (thread-safe use is the caller's job).
    void queue(json::Value command) { incoming_.push_back(std::move(command)); }
    /// Applies a command now. Returns false (with message) for unknown/invalid commands.
    bool apply(const json::Value& command, std::string& error);

    // ---- Queries for clients
    json::Value info_json() const;                         // map summary, config, population
    json::Value state_json(std::size_t recent_events = 15) const;
    json::Value map_json() const;
    json::Value agent_json(std::uint32_t id) const;
    std::vector<std::uint8_t> frame_binary() const;
    /// Bitmap (1 bit per resident) of residents matching the filter set.
    std::vector<std::uint8_t> query(const json::Value& filter, std::size_t* matches = nullptr) const;
    std::string export_csv(const json::Value& filter) const;
    std::string resident_name(std::uint32_t i) const;
    std::string building_label(std::uint32_t b) const;

    std::uint64_t checksum() const;
    SimState& state() { return state_; }
    const SimState& state() const { return state_; }
    Telemetry& telemetry() { return telemetry_; }
    const Telemetry& telemetry() const { return telemetry_; }
    const std::vector<Command>& command_log() const { return log_; }
    const world::MapLibrary& maps() const { return maps_; }
    const std::string& scenario_name() const { return scenario_name_; }

private:
    bool matches(std::uint32_t i, const json::Value& rule) const;

    const data::Catalog& cat_;
    const world::MapLibrary& maps_;
    SimState state_;
    Telemetry telemetry_;
    std::deque<json::Value> incoming_;
    std::vector<Command> scheduled_;   // future commands (scenario events / replay), sorted by tick
    std::size_t scheduled_next_ = 0;
    std::vector<Command> log_;         // every applied command
    std::string scenario_name_;
    json::Value names_;
};

} // namespace npc::sim
