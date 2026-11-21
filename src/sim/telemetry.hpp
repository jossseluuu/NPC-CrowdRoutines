#pragma once
// Time series sampled once per simulated minute: activities, needs, movement, traffic and events.

#include "sim/state.hpp"

#include <string>
#include <vector>

namespace npc::sim {

struct Sample {
    double t = 0;
    std::vector<std::uint32_t> activity;   // residents per main activity
    float stress = 0, fear = 0, energy = 0, hunger = 0, social = 0, fun = 0;
    std::uint32_t outdoor = 0, walking = 0, cycling = 0, driving = 0, bus = 0, waiting = 0, groups = 0;
    float walk_speed = 0, car_speed = 0, congestion = 0, km = 0;
    std::uint16_t events = 0;
    std::uint8_t weather = 0;
};

class Telemetry {
public:
    void clear() { samples_.clear(); }
    void sample(const SimState& s);
    const std::vector<Sample>& samples() const { return samples_; }
    /// Samples with t > since, at most max_points (evenly decimated).
    json::Value series(const SimState& s, double since, std::size_t max_points) const;
    bool write_csv(const SimState& s, const std::string& path) const;

private:
    std::vector<Sample> samples_;
};

} // namespace npc::sim
