#pragma once
// Data catalog: activities/substates, building types, districts, profiles, archetypes and events,
// loaded from data/*.json and validated. Everything the simulation decides is driven by this data.

#include "core/json.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace npc::data {

enum Need : int { Energy, Hunger, Hygiene, Social, Fun, Health, Stress, Fear, kNeedCount };
enum Trait : int {
    Sociability, Conscientiousness, Curiosity, RiskAversion, Patience, Aggression,
    Empathy, ActivityLevel, Thrift, Routine, NightOwl, Neuroticism, kTraitCount
};
using NeedArray = std::array<float, kNeedCount>;
using TraitArray = std::array<float, kTraitCount>;

extern const std::array<const char*, kNeedCount> kNeedNames;
extern const std::array<const char*, kTraitCount> kTraitNames;

struct HourWindow {
    float start = 0, end = 24, bonus = 0;
    bool contains(double h) const;
};

enum class LocationKind : std::uint8_t { Home, Travel, Work, School, Venues, Anywhere };

struct Substate {
    std::string id;
    int activity = -1;
    float weight = 1.f;
    std::vector<HourWindow> hours;  // empty = any time
    std::vector<int> venues;        // building categories; empty = not venue-bound
    bool automatic = false;         // assigned by the engine from context
    NeedArray needs{};              // extra weight when these needs are urgent
    TraitArray traits{};            // weight multipliers by personality
};

struct Activity {
    std::string id, color, icon;
    int index = -1;
    std::vector<int> substates;     // global substate indices
    LocationKind location = LocationKind::Anywhere;
    std::vector<int> venues;        // building categories for LocationKind::Venues
    bool friend_home = false, family_home = false, own_home = false; // special venue tokens
    bool choosable = false, scheduled = false, outdoor = false, weekday_only = false;
    float min_minutes = 15, max_minutes = 120, base = 0, weekend_bonus = 0;
    NeedArray need_weights{}, effects{};
    TraitArray trait_mult{};
    std::vector<HourWindow> hours;
};

struct DayHours {
    float open = -1, close = -1; // open < 0 = closed all day
    bool closed() const { return open < 0; }
};

struct BuildingType {
    std::string id, color;
    int index = -1, category = -1;
    float size[4] = {10, 10, 10, 10};
    int floors[2] = {1, 1};
    int households[2] = {0, 0};
    int jobs[2] = {0, 0};
    int students[2] = {0, 0};
    int visitors = 0;
    bool open24h = false;
    std::array<DayHours, 7> hours{}; // 0 = Monday
    bool residential() const { return households[1] > 0; }
    bool is_open(int weekday, double hour) const;
};

struct DistrictKind {
    std::string id, color;
    int index = -1;
    float density = 0.5f;
    std::vector<std::pair<int, float>> weights; // building type index -> weight
};

enum class WorkDays : std::uint8_t { Weekdays, Six, Rotating, Random, None };

struct Profile {
    std::string id;
    int index = -1;
    float weight = 1;
    int age[2] = {18, 65};
    std::vector<int> workplaces; // categories; may include the "home" category
    WorkDays days = WorkDays::Weekdays;
    float start_mean = 9, start_sd = 1, duration_mean = 8, duration_sd = 0.5f;
    std::vector<float> shifts;
    std::array<float, 4> commute{0.25f, 0.25f, 0.25f, 0.25f}; // walk, bike, bus, car
    bool student = false, care = false;
};

struct Archetype {
    std::string id;
    int index = -1;
    TraitArray traits{};
    std::vector<float> profile_affinity; // per profile, default 1
    std::vector<float> prefs;            // per activity multiplier, default 1
    bool has_commute = false;
    std::array<float, 4> commute{};
};

enum class EventLocation : std::uint8_t { Road, Building, District, Anywhere, Citywide };
enum class Weather : std::uint8_t { Clear, Rain, Storm, Heat, kCount };
extern const std::array<const char*, static_cast<int>(Weather::kCount)> kWeatherNames;

struct EventEffects {
    NeedArray needs{};          // per hour inside the area
    float walk_speed = 1, car_speed = 1, attract = 0, outdoor_penalty = 0;
    bool block_roads = false, close_buildings = false, evacuate = false, conflict = false, bus_delay = false;
    int crowd[2] = {0, 0};
    int weather = -1;           // Weather index or -1
};

struct EventResponse { float watch = 0, investigate = 0, flee = 0, evacuate = 0, shelter = 0, help = 0; };
struct EventPerception { float see = 1, hear = 0, spread = 0, alert_minutes = -1; };

struct EventType {
    std::string id, category, color, icon;
    int index = -1;
    float base_rate = 0;        // per simulated day at chaos 1
    std::vector<HourWindow> hours;
    float weekend = 1;
    EventLocation location = EventLocation::Anywhere;
    std::vector<int> categories, districts;
    float radius[2] = {30, 30}, duration[2] = {30, 30}, severity[2] = {0.5f, 0.5f};
    float recovery = 0;         // minutes
    EventEffects effects;
    EventResponse response;
    EventPerception perception;
    float hour_multiplier(double hour) const;
};

class Catalog {
public:
    /// Loads every catalog file from `data_dir`. Returns false and fills `error` on failure.
    bool load(const std::string& data_dir, std::string& error);

    std::vector<Activity> activities;
    std::vector<Substate> substates;
    std::vector<std::string> categories;      // building categories (venue kinds); "home" always exists
    std::vector<BuildingType> building_types;
    std::vector<DistrictKind> districts;
    std::vector<std::pair<int, int>> minimums; // building type, count
    std::vector<Profile> profiles;
    std::vector<Archetype> archetypes;
    std::vector<EventType> events;
    NeedArray base_drift{};
    float trait_sd = 0.1f;
    std::vector<float> household_size_weights;
    std::string data_dir;

    int activity_index(std::string_view id) const { return lookup(activity_ids_, id); }
    int substate_index(std::string_view id) const { return lookup(substate_ids_, id); }
    int category_index(std::string_view id) const { return lookup(category_ids_, id); }
    int building_index(std::string_view id) const { return lookup(building_ids_, id); }
    int district_index(std::string_view id) const { return lookup(district_ids_, id); }
    int profile_index(std::string_view id) const { return lookup(profile_ids_, id); }
    int archetype_index(std::string_view id) const { return lookup(archetype_ids_, id); }
    int event_index(std::string_view id) const { return lookup(event_ids_, id); }

    int home_category = -1;
    int act_travelling = -1, act_emergency = -1, act_sleeping = -1, act_home = -1, act_working = -1,
        act_studying = -1, act_social = -1, act_care = -1, act_health = -1;
    /// Substate index by id that must exist (engine-assigned substates); -1 if missing.
    int sub(std::string_view id) const { return substate_index(id); }

    /// Catalog as JSON for clients (ids, colors, hierarchy). Names come from data/i18n.
    json::Value to_json() const;

private:
    using Index = std::unordered_map<std::string, int>;
    static int lookup(const Index& idx, std::string_view id) {
        const auto it = idx.find(std::string(id));
        return it == idx.end() ? -1 : it->second;
    }
    bool load_activities(const json::Value& v, std::string& err);
    bool load_buildings(const json::Value& v, std::string& err);
    bool load_population(const json::Value& v, std::string& err);
    bool load_events(const json::Value& v, std::string& err);
    int ensure_category(const std::string& id);

    Index activity_ids_, substate_ids_, category_ids_, building_ids_, district_ids_, profile_ids_, archetype_ids_, event_ids_;
};

} // namespace npc::data
