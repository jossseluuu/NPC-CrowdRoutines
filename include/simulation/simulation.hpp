#pragma once

#include "core/types.hpp"
#include "world/spatial_grid.hpp"
#include <chrono>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

namespace npc_crowd {
class Simulation {
public:
    explicit Simulation(Config config);
    Metrics run();
    std::size_t population() const { return config_.population; }
    std::uint32_t tick() const { return tick_; }
    double world_hour() const { return world_hour_; }
    std::size_t event_count() const { return events_.size(); }
    std::string_view backend_name() const { return config_.backend==Backend::Soa ? "SoA" : "OOP"; }
private:
    struct Soa {
        std::vector<Vec2> position,velocity,home,workplace,destination;
        std::vector<float> energy,hunger,stress,social,bladder,comfort,health,fatigue,fear,curiosity,money,temperature;
        std::vector<float> sociability,risk_aversion,patience,conscientiousness;
        std::vector<float> wake_hour,sleep_hour,work_start_hour,work_duration,breakfast_hour,breakfast_probability;
        std::vector<std::uint16_t> age;
        std::vector<std::uint16_t> workday_mask;
        std::vector<std::uint8_t> job_type,breakfast_venue,commute_mode,sex,profile;
        std::vector<Activity> activity;
        std::size_t size() const { return position.size(); }
    };
    void initialize();
    void step();
    void spawn_events();
    void update_agent(Agent& agent);
    void update_soa(std::size_t index);
    Activity choose_activity(float energy,float hunger,float stress,float wake,float sleep,float work_start,float work_duration,
                             float breakfast_time,float breakfast_probability,std::uint16_t workdays) const;
    Config config_;
    std::mt19937_64 rng_;
    std::uint32_t tick_{};
    double world_hour_{6.0};
    std::uint8_t week_day_{1};
    SpatialGrid grid_;
    std::vector<std::unique_ptr<Agent>> oop_;
    Soa soa_;
    std::vector<Event> events_;
};
} // namespace npc_crowd
