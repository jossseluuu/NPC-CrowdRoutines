#pragma once
// Shared simulation state. Subsystems (behavior, movement, events, telemetry) operate on SimState;
// Simulation (simulation.hpp) owns it and drives the fixed-step loop.

#include "core/clock.hpp"
#include "core/json.hpp"
#include "core/rng.hpp"
#include "data/catalog.hpp"
#include "population/agents.hpp"
#include "world/city_map.hpp"
#include "world/navigation.hpp"

#include <memory>
#include <string>
#include <vector>

namespace npc::sim {

/// Everything needed to reproduce a run from scratch (part of scenarios and saved runs).
struct Config {
    std::string name = "Sandbox";
    std::uint64_t seed = 42;
    int population = 0;                 // 0 = map suggestion
    std::string map_template = "grid";
    std::string map_size = "small";
    std::uint64_t map_seed = 0;         // 0 = same as seed
    std::string map_file;               // custom map (overrides template/size)
    double start_hour = 7.0;
    int start_weekday = 0;              // 0 = Monday
    double step_seconds = 1.0;
    float chaos = 1.0f;                 // random event rate multiplier (0 disables)
    bool random_events = true;

    json::Value to_json() const;
    static Config from_json(const json::Value& v);
};

struct Bus {
    std::uint32_t line = 0;
    float s = 0;                        // distance along the line path
    int dir = 1;                        // +1 forward, -1 backward
    float dwell = 0;                    // seconds left at the current stop
    std::uint32_t at_stop = pop::kNone; // path index of the stop while dwelling
    Vec2 pos{};
    std::uint16_t passengers = 0;
    float delay = 0;                    // seconds of breakdown left
};

struct BusLineGeom {
    std::vector<float> cumulative;      // distance at each path node
    std::vector<std::uint8_t> is_stop;  // per path index
    float length = 0;
};

enum class EventPhase : std::uint8_t { Active, Recovering, Ended };

struct EventInstance {
    std::int32_t id = 0;
    int type = 0;
    Vec2 pos{};
    float radius = 0, severity = 0;
    double start = 0, end = 0, recovery_end = 0;
    EventPhase phase = EventPhase::Active;
    bool manual = false;
    std::uint32_t building = pop::kNone;
    int district = -1;
    std::vector<std::uint32_t> blocked_edges, closed_buildings;
    std::uint32_t perceived = 0, reacted = 0, crowd_target = 0, crowd_joined = 0;
    bool alert_sent = false;
};

struct Weather {
    data::Weather kind = data::Weather::Clear;
    float walk = 1, car = 1, outdoor_penalty = 0;
};

struct SimState {
    explicit SimState(const data::Catalog& c) : cat(c) {}

    const data::Catalog& cat;
    Config config;
    SimClock clock;
    world::CityMap map;
    std::unique_ptr<world::Router> router;
    pop::Agents agents;

    // Independent random streams (see core/rng.hpp): population generation is done before the loop.
    Rng rng_behavior, rng_events, rng_social;

    // Buildings
    std::vector<std::uint16_t> occupancy;      // residents inside each building
    std::vector<std::uint8_t> closed;          // closed by an event
    std::vector<float> attraction;             // crowd-event pull per building (0 = none)

    // Transport
    std::vector<Bus> buses;
    std::vector<BusLineGeom> lines;
    std::vector<std::uint16_t> cars_on_edge, cars_next;
    std::vector<float> edge_speed_factor;      // events: car speed multiplier per edge
    std::vector<float> walk_factor_edge;       // events: walk speed multiplier per edge
    float light_cycle = 70.f;                  // seconds; NS green, EW green halves

    // Events
    std::vector<EventInstance> events;          // active and recently ended
    std::vector<EventInstance> event_history;   // ended (capped)
    std::int32_t next_event_id = 1;
    Weather weather;

    double now() const { return clock.seconds(); }
    double hour() const { return clock.hour(); }
    float dt() const { return static_cast<float>(clock.step_seconds); }
    const EventInstance* find_event(std::int32_t id) const;
    bool building_open(std::uint32_t b) const;
    /// Light state for a car/pedestrian moving along direction d at node n.
    bool green_for(std::uint32_t node, Vec2 d) const;
};

} // namespace npc::sim
