#pragma once
// City events: random generation, lifecycle, area effects, perception, social propagation and reactions.

#include "sim/state.hpp"

#include <vector>

namespace npc::events {

/// Creates an event of catalog type `type`. If `pos` has x < 0 the location is chosen from the type's rules.
/// Optional overrides (< 0 = sample from the catalog ranges). Returns the event id, or -1 if invalid.
std::int32_t spawn(sim::SimState& s, int type, Vec2 pos, bool manual, std::vector<std::uint32_t> involved = {},
                   float radius = -1, float duration_minutes = -1, float severity = -1);

/// Ends an event immediately (manual command).
void end_event(sim::SimState& s, std::int32_t id);

/// Generation, lifecycle, effects, perception and reactions. Call once per tick.
void update(sim::SimState& s, float dt);

json::Value event_json(const sim::SimState& s, const sim::EventInstance& e);

} // namespace npc::events
