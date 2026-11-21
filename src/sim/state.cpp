#include "sim/state.hpp"

#include <cmath>

namespace npc::sim {

json::Value Config::to_json() const {
    json::Value v = json::Value::object();
    v.set("name", name);
    v.set("seed", static_cast<double>(seed));
    v.set("population", population);
    v.set("map_template", map_template);
    v.set("map_size", map_size);
    v.set("map_seed", static_cast<double>(map_seed));
    if (!map_file.empty()) v.set("map_file", map_file);
    v.set("start_hour", start_hour);
    v.set("start_weekday", start_weekday);
    v.set("step_seconds", step_seconds);
    v.set("chaos", chaos);
    v.set("random_events", random_events);
    return v;
}

Config Config::from_json(const json::Value& v) {
    Config c;
    c.name = v.str("name", c.name);
    c.seed = static_cast<std::uint64_t>(v.num("seed", static_cast<double>(c.seed)));
    c.population = static_cast<int>(v.integer("population", c.population));
    c.map_template = v.str("map_template", c.map_template);
    c.map_size = v.str("map_size", c.map_size);
    c.map_seed = static_cast<std::uint64_t>(v.num("map_seed", 0));
    c.map_file = v.str("map_file", "");
    c.start_hour = v.num("start_hour", c.start_hour);
    c.start_weekday = static_cast<int>(v.integer("start_weekday", c.start_weekday)) % 7;
    c.step_seconds = std::clamp(v.num("step_seconds", c.step_seconds), 0.25, 5.0);
    c.chaos = std::clamp(v.flt("chaos", c.chaos), 0.f, 10.f);
    c.random_events = v.boolean("random_events", c.random_events);
    return c;
}

const EventInstance* SimState::find_event(std::int32_t id) const {
    if (id < 0) return nullptr;
    for (const auto& e : events)
        if (e.id == id) return &e;
    return nullptr;
}

bool SimState::building_open(std::uint32_t b) const { return b < closed.size() && closed[b] == 0; }

bool SimState::green_for(std::uint32_t node, Vec2 d) const {
    // Fixed-time controller with a per-node offset; north-south gets the first half of the cycle.
    const double phase = std::fmod(now() + static_cast<double>(node) * 7.3, static_cast<double>(light_cycle));
    const bool ns_green = phase < light_cycle * 0.5;
    const bool moving_ns = std::abs(d.y) >= std::abs(d.x);
    return ns_green == moving_ns;
}

} // namespace npc::sim
