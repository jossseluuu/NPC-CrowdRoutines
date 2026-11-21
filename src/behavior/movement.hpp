#pragma once
// Trips and movement: walking on sidewalks, cycling, driving with congestion and traffic lights,
// and bus lines with stops, boarding and alighting.

#include "sim/state.hpp"

namespace npc::movement {

/// Prepares bus fleets and per-edge traffic buffers for the loaded map.
void init_transport(sim::SimState& s);

/// Plans a trip for resident i to a building (or to a point when target_building == kNone).
/// Returns false if no route exists; the resident then stays where it is.
bool start_trip(sim::SimState& s, std::uint32_t i, std::uint32_t target_building, Vec2 target_point, world::TravelMode mode);

/// Makes `follower` travel with `leader` (same legs, lockstep), e.g. walking and talking together.
void join_trip(sim::SimState& s, std::uint32_t follower, std::uint32_t leader);

/// Stops a trip in place (the resident stays outdoors at its current position).
void cancel_trip(sim::SimState& s, std::uint32_t i);

/// Advances buses, traffic and every travelling resident by dt seconds.
void step(sim::SimState& s, float dt);

/// Walking speed multiplier from events/weather at the resident's position.
float walk_factor(const sim::SimState& s, std::uint32_t i);

} // namespace npc::movement
