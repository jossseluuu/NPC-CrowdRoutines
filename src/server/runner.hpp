#pragma once
// Runs a Simulation in a background thread at a chosen speed and publishes snapshots for the HTTP API.

#include "sim/simulation.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace npc::server {

struct RunnerStatus {
    bool running = false;
    double speed = 60;          // requested simulated seconds per real second
    double achieved = 0;        // measured simulated seconds per real second
    double tick_ms_p50 = 0, tick_ms_p95 = 0, tick_ms_p99 = 0, tick_ms_max = 0;
    std::uint64_t ticks = 0;
};

class SimRunner {
public:
    SimRunner(const data::Catalog& cat, const world::MapLibrary& maps);
    ~SimRunner();

    void start();
    void stop();

    void set_running(bool on) { running_ = on; }
    void set_speed(double s);
    /// Advances `ticks` steps while paused.
    void step(std::uint64_t ticks);

    /// Runs `fn` with exclusive access to the simulation (keep it short).
    template <class F>
    auto with_sim(F&& fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        return fn(sim_);
    }
    /// Replaces the simulation (reset/scenario/run load) and refreshes cached data.
    bool rebuild(const std::function<bool(sim::Simulation&, std::string&)>& fn, std::string& error);

    std::vector<std::uint8_t> frame() const;
    std::string state() const;
    std::string map() const;
    RunnerStatus status() const;

private:
    void loop();
    void publish(); // requires mutex_

    sim::Simulation sim_;
    mutable std::mutex mutex_;
    mutable std::mutex cache_mutex_;
    std::thread thread_;
    std::atomic<bool> alive_{false}, running_{false};
    std::atomic<double> speed_{60.0};
    std::atomic<std::uint64_t> pending_steps_{0};
    std::vector<std::uint8_t> frame_;
    std::string state_, map_;
    std::vector<float> tick_ms_; // ring of recent tick durations
    std::size_t tick_head_ = 0;
    double achieved_ = 0;
    std::uint64_t ticks_ = 0;
};

} // namespace npc::server
