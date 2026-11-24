#pragma once

#include "sim/state.hpp"

namespace npc::behavior {

    void update_needs(sim::SimState& s, float dt);
    void decide(sim::SimState& s, std::uint32_t i);
    void on_arrival(sim::SimState& s, std::uint32_t i);
    void set_state(sim::SimState& s, std::uint32_t i, int activity, int substate);
    int pick_substate(sim::SimState& s, std::uint32_t i, int activity, int venue_category);
    void refresh_substates(sim::SimState& s);
    void encounters(sim::SimState& s);
    Vec2 interior_point(const sim::SimState& s, std::uint32_t i, std::uint32_t b);
    json::Value explain(sim::SimState& s, std::uint32_t i);

}