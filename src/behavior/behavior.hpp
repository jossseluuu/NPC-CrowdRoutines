#pragma once
// Resident behaviour: needs, utility-based activity selection, venue choice, substates, groups and encounters.

#include "sim/state.hpp"

namespace npc::behavior {

/// Advances the needs of every resident by dt seconds.
void update_needs(sim::SimState& s, float dt);

/// Utility decision for resident i: chooses an activity and a venue and either starts it or starts a trip.
void decide(sim::SimState& s, std::uint32_t i);

/// Called by the movement system when resident i reaches the destination of its trip.
void on_arrival(sim::SimState& s, std::uint32_t i);

/// Sets activity/substate, records history and keeps occupancy counters consistent.
void set_state(sim::SimState& s, std::uint32_t i, int activity, int substate);

/// Chooses the substate of `activity` for resident i given where it happens (data-driven weights).
int pick_substate(sim::SimState& s, std::uint32_t i, int activity, int venue_category);

/// Re-rolls substates of residents whose substate timer expired (e.g. cooking -> eating -> watching TV).
void refresh_substates(sim::SimState& s);

/// Street encounters between acquaintances (conversations, arguments). Call every few ticks.
void encounters(sim::SimState& s);

/// Interior position for resident i inside building b (deterministic, spread over the footprint).
Vec2 interior_point(const sim::SimState& s, std::uint32_t i, std::uint32_t b);

/// Score breakdown of every activity for resident i at the current time (inspector "why" panel).
json::Value explain(sim::SimState& s, std::uint32_t i);

} // namespace npc::behavior
