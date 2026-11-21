#pragma once
// Resident state stored as structure-of-arrays (one contiguous vector per field) for cache-friendly updates.

#include "core/math.hpp"
#include "data/catalog.hpp"
#include "world/navigation.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace npc::pop {

constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();
constexpr int kHistory = 48;  // state transitions kept per resident
constexpr int kActions = 32;  // notable actions kept per resident
constexpr int kKnownEvents = 6;

enum class Sex : std::uint8_t { Male, Female, Other };
enum class RelKind : std::uint8_t { Family, Partner, Friend, Coworker, Classmate, Neighbour, Rival, kCount };
const char* rel_kind_name(RelKind k);

struct Relation {
    std::uint32_t other;
    RelKind kind;
    float bond; // 0..1
};

struct AgendaItem {
    float start = 0, end = 0;     // hours of the current day (end may exceed 24)
    std::uint8_t activity = 0;
    bool hard = false;            // scheduled obligation (work, school, care)
    std::uint32_t building = kNone;
};

enum class ActionKind : std::uint16_t {
    Woke, WentToSleep, Arrived, Chatted, Argued, JoinedGroup, PerceivedEvent, ToldAbout, Evacuated, Fled,
    Helped, Watched, Sheltered, Boarded, ChangedPlan, Pursued, kCount
};
const char* action_kind_name(ActionKind k);

struct HistoryEntry {
    double t = -1;
    std::uint8_t activity = 0, substate = 0;
    std::uint32_t building = kNone;
};

struct ActionEntry {
    double t = -1;
    ActionKind kind = ActionKind::Arrived;
    std::uint32_t a = kNone, b = kNone; // meaning depends on kind (other resident, building, event id)
};

enum class LegKind : std::uint8_t { Walk, Bike, Drive, Wait, Ride };

struct Leg {
    LegKind kind = LegKind::Walk;
    world::Route route;
    std::uint32_t line = kNone;       // bus line for Wait/Ride
    std::uint32_t board_stop = 0, alight_stop = 0; // indices into the line's path
};

struct MoveState {
    std::array<Leg, 4> legs;
    std::uint8_t leg_count = 0, leg = 0;
    std::uint32_t edge_index = 0;     // position within the current leg's route
    std::uint32_t at_node = 0;        // node at the start of the current edge
    float t = 0;                      // progress along the current edge
    float wait = 0;                   // seconds waiting (lights, bus)
    std::uint32_t bus = kNone;
    std::uint32_t target_building = kNone;
    Vec2 target_point{};              // used when target_building == kNone
    bool active = false;
};

struct Agents {
    std::size_t size() const { return home.size(); }
    void resize(std::size_t n);

    // Identity
    std::vector<std::uint8_t> profile, archetype, archetype2, age;
    std::vector<Sex> sex;
    std::vector<std::uint32_t> household, home, work;
    std::vector<world::TravelMode> commute;
    std::vector<data::TraitArray> traits;
    std::vector<std::uint8_t> workdays; // bit d = works on weekday d

    // Schedule parameters (hours)
    std::vector<float> work_start, work_duration, wake, sleep;
    std::vector<std::vector<AgendaItem>> agenda;
    std::vector<int> agenda_day;

    // Relationships (CSR)
    std::vector<std::uint32_t> rel_offset; // size n + 1
    std::vector<Relation> rels;

    // Dynamic state
    std::vector<data::NeedArray> needs;
    std::vector<std::uint8_t> activity, substate, pending_activity;
    std::vector<double> since, commit_until, next_decision, next_substate, cooldown;
    std::vector<std::uint32_t> location;          // building the resident is in (kNone = outdoors)
    std::vector<Vec2> pos;
    std::vector<std::uint8_t> outdoor;            // 1 = visible on the street / in open spaces
    std::vector<MoveState> move;
    std::vector<std::uint32_t> group;             // leader id (self if leader), kNone = alone
    std::vector<std::uint32_t> partner;           // conversation / pursuit partner
    std::vector<float> distance;                  // metres travelled
    std::vector<float> speed;                     // current movement speed (m/s)
    std::vector<std::array<std::int32_t, kKnownEvents>> known_events;
    std::vector<std::int32_t> reacting_to;        // event id driving an emergency reaction, -1 none

    // Ring buffers
    std::vector<HistoryEntry> history;            // n * kHistory
    std::vector<std::uint8_t> history_head;
    std::vector<ActionEntry> actions;             // n * kActions
    std::vector<std::uint8_t> actions_head;

    // Helpers
    std::size_t rel_begin(std::uint32_t i) const { return rel_offset[i]; }
    std::size_t rel_end(std::uint32_t i) const { return rel_offset[i + 1]; }
    const Relation* relation(std::uint32_t a, std::uint32_t b) const;
    void record_state(std::uint32_t i, double t);
    void log(std::uint32_t i, double t, ActionKind kind, std::uint32_t a = kNone, std::uint32_t b = kNone);
    bool knows(std::uint32_t i, std::int32_t event) const;
    bool learn(std::uint32_t i, std::int32_t event); // true if new
};

} // namespace npc::pop
