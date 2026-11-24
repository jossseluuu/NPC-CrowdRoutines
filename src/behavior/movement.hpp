#pragma once

#include "sim/state.hpp"

namespace npc::movement {
    
    void init_transport(sim::SimState& s);
    bool start_trip(sim::SimState& s, std::uint32_t i, std::uint32_t target_building, Vec2 target_point, world::TravelMode mode);
    void join_trip(sim::SimState& s, std::uint32_t follower, std::uint32_t leader);
    void cancel_trip(sim::SimState& s, std::uint32_t i);
    void step(sim::SimState& s, float dt);
    float walk_factor(const sim::SimState& s, std::uint32_t i);

}