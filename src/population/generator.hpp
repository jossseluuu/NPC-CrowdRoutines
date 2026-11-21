#pragma once
// Population synthesis (households, profiles, personalities, schedules, workplaces, social network)
// and daily agenda planning.

#include "sim/state.hpp"

namespace npc::pop {

/// Creates `count` residents living in the map's homes. Deterministic for a given seed.
void generate_population(sim::SimState& s, std::size_t count, std::uint64_t seed);

/// Builds the agenda of resident i for simulated day `day` (scheduled obligations + soft plans).
void build_agenda(sim::SimState& s, std::uint32_t i, int day);

/// Hours of commute expected between two buildings with a travel mode (rough, for planning only).
float estimate_travel_hours(const sim::SimState& s, std::uint32_t from_building, std::uint32_t to_building, world::TravelMode mode);

} // namespace npc::pop
