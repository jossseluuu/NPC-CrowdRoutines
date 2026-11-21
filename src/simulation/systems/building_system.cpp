#include "building_system.hpp"

#include <algorithm>
#include <cmath>

namespace npc_crowd::simulation::systems {

BuildingSystem::BuildingSystem(const world::BuildingRegistry& registry, const world::ZoneRegistry& zone_registry)
    : building_registry_(registry), zone_registry_(zone_registry) {}

void BuildingSystem::initialize() {
    occupancy_states_.clear();
    agent_building_.clear();
    agent_zone_.clear();
    stats = SystemStats{};

    for (const auto& [id, building] : building_registry_.buildings) {
        BuildingOccupancyState state;
        state.building_id = id;
        state.max_capacity = building.max_capacity;
        state.condition = building.condition;
        state.is_open = building.is_open(world_hour_, week_day_, day_of_year_);
        for (const auto& zone : building.zones) state.zone_occupancy[zone.id] = 0;
        occupancy_states_.emplace(id, std::move(state));
        stats.capacity_by_type[building.type] += building.max_capacity;
    }
    stats.total_buildings = occupancy_states_.size();
}

void BuildingSystem::set_clock(float world_hour, int week_day, int day_of_year) {
    world_hour_ = world_hour;
    week_day_ = week_day;
    day_of_year_ = day_of_year;
    for (auto& [id, state] : occupancy_states_) {
        const auto* building = building_registry_.get(id);
        if (!building) continue;
        state.is_open = building_open(*building, state);
        if (building->is_24h) {
            state.time_until_open = 0.f;
            state.time_until_close = -1.f;
        } else {
            state.time_until_open = building->hours.time_until_open(world_hour_, week_day_, day_of_year_);
            state.time_until_close = building->hours.time_until_close(world_hour_, week_day_, day_of_year_);
        }
    }
}

bool BuildingSystem::building_open(const world::Building& building, const BuildingOccupancyState& state) const {
    return !state.evacuated && building.is_open(world_hour_, week_day_, day_of_year_);
}

void BuildingSystem::refresh_ratio(BuildingOccupancyState& state) const {
    state.occupancy_ratio = state.max_capacity > 0
                                ? static_cast<float>(state.current_occupancy) / static_cast<float>(state.max_capacity)
                                : 0.f;
}

BuildingOperationResult BuildingSystem::enter_building(std::uint32_t agent_id, std::uint32_t target_building_id,
                                                       std::uint32_t target_zone_id, bool authorized) {
    ++stats.total_enter_attempts;

    const world::Building* building = building_registry_.get(target_building_id);
    auto state_it = occupancy_states_.find(target_building_id);
    if (!building || state_it == occupancy_states_.end()) return BuildingOperationResult::BuildingNotFound;
    auto& state = state_it->second;

    if (!building_open(*building, state)) return BuildingOperationResult::Closed;
    if (state.current_occupancy >= state.max_capacity) return BuildingOperationResult::AtCapacity;

    if (target_zone_id != 0) {
        const auto zone_it = std::find_if(building->zones.begin(), building->zones.end(),
                                          [&](const world::InteriorZone& z) { return z.id == target_zone_id; });
        if (zone_it == building->zones.end()) return BuildingOperationResult::InvalidZone;
        if (zone_it->is_restricted && !authorized) return BuildingOperationResult::Restricted;
        if (state.zone_occupancy[target_zone_id] >= zone_it->capacity) return BuildingOperationResult::AtCapacity;
    }

    // Si ya estaba en otro edificio, sale primero para no contar al agente dos veces.
    if (auto prev = agent_building_.find(agent_id); prev != agent_building_.end()) {
        if (prev->second == target_building_id) return BuildingOperationResult::Success;
        exit_building(agent_id, prev->second);
    }

    ++state.current_occupancy;
    state.agents_inside.push_back(agent_id);
    agent_building_[agent_id] = target_building_id;
    if (target_zone_id != 0) {
        ++state.zone_occupancy[target_zone_id];
        agent_zone_[agent_id] = target_zone_id;
    }
    refresh_ratio(state);
    ++stats.occupancy_by_type[building->type];
    ++stats.total_enter_successes;
    return BuildingOperationResult::Success;
}

BuildingOperationResult BuildingSystem::exit_building(std::uint32_t agent_id, std::uint32_t target_building_id) {
    ++stats.total_exit_attempts;

    const auto it = agent_building_.find(agent_id);
    if (it == agent_building_.end() || it->second != target_building_id) return BuildingOperationResult::AgentNotFound;
    auto& state = occupancy_states_.at(target_building_id);

    if (auto zone = agent_zone_.find(agent_id); zone != agent_zone_.end()) {
        auto& count = state.zone_occupancy[zone->second];
        if (count > 0) --count;
        agent_zone_.erase(zone);
    }
    if (auto pos = std::find(state.agents_inside.begin(), state.agents_inside.end(), agent_id);
        pos != state.agents_inside.end())
        state.agents_inside.erase(pos);
    if (state.current_occupancy > 0) --state.current_occupancy;
    refresh_ratio(state);
    if (const auto* building = building_registry_.get(target_building_id)) {
        auto& count = stats.occupancy_by_type[building->type];
        if (count > 0) --count;
    }
    agent_building_.erase(it);

    ++stats.total_exit_successes;
    return BuildingOperationResult::Success;
}

BuildingOccupancyState BuildingSystem::get_occupancy(std::uint32_t building_id) const {
    if (auto it = occupancy_states_.find(building_id); it != occupancy_states_.end()) return it->second;
    BuildingOccupancyState empty;
    empty.building_id = building_id;
    return empty;
}

std::uint32_t BuildingSystem::get_building_for_agent(std::uint32_t agent_id) const {
    const auto it = agent_building_.find(agent_id);
    return it != agent_building_.end() ? it->second : 0;
}

std::uint32_t BuildingSystem::get_zone_for_agent(std::uint32_t agent_id) const {
    const auto it = agent_zone_.find(agent_id);
    return it != agent_zone_.end() ? it->second : 0;
}

const world::Building* BuildingSystem::get_building_definition(std::uint32_t building_id) const {
    return building_registry_.get(building_id);
}

const world::InteriorZone* BuildingSystem::get_zone_definition(std::uint32_t building_id, std::uint32_t zone_id) const {
    const auto* building = building_registry_.get(building_id);
    if (!building) return nullptr;
    for (const auto& zone : building->zones)
        if (zone.id == zone_id) return &zone;
    return nullptr;
}

std::vector<std::uint32_t> BuildingSystem::find_open_of_types(std::initializer_list<world::BuildingType> types,
                                                              std::uint32_t min_capacity) const {
    std::vector<std::uint32_t> result;
    for (const auto& [id, state] : occupancy_states_) {
        const auto* building = building_registry_.get(id);
        if (!building || std::find(types.begin(), types.end(), building->type) == types.end()) continue;
        if (!building_open(*building, state)) continue;
        if (state.current_occupancy + min_capacity <= state.max_capacity) result.push_back(id);
    }
    std::sort(result.begin(), result.end()); // orden determinista (unordered_map no lo garantiza)
    return result;
}

std::vector<std::uint32_t> BuildingSystem::find_open_hospitals(std::uint32_t min_capacity) const {
    return find_open_of_types({world::BuildingType::Hospital}, min_capacity);
}

std::vector<std::uint32_t> BuildingSystem::find_open_schools(std::uint32_t min_capacity) const {
    using T = world::BuildingType;
    return find_open_of_types({T::Kindergarten, T::PrimarySchool, T::SecondarySchool, T::HighSchool, T::VocationalSchool},
                              min_capacity);
}

std::vector<std::uint32_t> BuildingSystem::find_open_offices(std::uint32_t min_capacity) const {
    using T = world::BuildingType;
    return find_open_of_types({T::OfficeSmall, T::OfficeMedium, T::OfficeLarge, T::CorporateHQ, T::Coworking},
                              min_capacity);
}

std::vector<std::uint32_t> BuildingSystem::find_open_police_stations(std::uint32_t min_capacity) const {
    return find_open_of_types({world::BuildingType::PoliceStation}, min_capacity);
}

std::vector<std::uint32_t> BuildingSystem::find_open_fire_stations(std::uint32_t min_capacity) const {
    return find_open_of_types({world::BuildingType::FireStation}, min_capacity);
}

std::vector<std::uint32_t> BuildingSystem::find_open_universities(std::uint32_t min_capacity) const {
    return find_open_of_types({world::BuildingType::University}, min_capacity);
}

std::vector<std::uint32_t> BuildingSystem::find_buildings_in_zone(std::uint32_t zone_id) const {
    const auto* zone = zone_registry_.get(zone_id);
    if (!zone) return {};
    std::vector<std::uint32_t> result(zone->building_ids.begin(), zone->building_ids.end());
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::uint32_t> BuildingSystem::find_open_buildings_in_zone(std::uint32_t zone_id) const {
    std::vector<std::uint32_t> result;
    for (auto id : find_buildings_in_zone(zone_id)) {
        const auto it = occupancy_states_.find(id);
        const auto* building = building_registry_.get(id);
        if (it == occupancy_states_.end() || !building) continue;
        if (building_open(*building, it->second) && it->second.current_occupancy < it->second.max_capacity)
            result.push_back(id);
    }
    return result;
}

void BuildingSystem::update_maintenance(float dt) {
    stats.total_buildings = occupancy_states_.size();
    stats.total_capacity = 0;
    stats.total_occupancy = 0;
    float total_ratio = 0.f;

    for (auto& [id, state] : occupancy_states_) {
        // El uso intenso desgasta más el edificio que tenerlo vacío.
        const float usage = 0.5f + state.occupancy_ratio;
        state.condition = std::max(0.f, state.condition - state.maintenance_rate * usage * dt);
        stats.total_capacity += state.max_capacity;
        stats.total_occupancy += state.current_occupancy;
        total_ratio += state.occupancy_ratio;
    }
    stats.avg_occupancy_ratio = occupancy_states_.empty() ? 0.f : total_ratio / static_cast<float>(occupancy_states_.size());
}

void BuildingSystem::repair_building(std::uint32_t building_id) {
    if (auto it = occupancy_states_.find(building_id); it != occupancy_states_.end()) it->second.condition = 1.f;
}

void BuildingSystem::set_building_condition(std::uint32_t building_id, float condition) {
    if (auto it = occupancy_states_.find(building_id); it != occupancy_states_.end())
        it->second.condition = std::clamp(condition, 0.f, 1.f);
}

std::vector<std::uint32_t> BuildingSystem::inject_event(EventType type, Vec2 position, float radius) {
    std::vector<std::uint32_t> evacuated_agents;
    if (type != EventType::Alarm) return evacuated_agents; // tráfico y lluvia no desalojan edificios

    const float r2 = radius * radius;
    for (auto& [id, state] : occupancy_states_) {
        const auto* building = building_registry_.get(id);
        if (!building || length_sq(building->position - position) > r2) continue;
        state.evacuated = true;
        state.is_open = false;
        const auto inside = state.agents_inside; // copia: exit_building modifica la lista
        for (auto agent : inside) {
            exit_building(agent, id);
            evacuated_agents.push_back(agent);
        }
    }
    std::sort(evacuated_agents.begin(), evacuated_agents.end());
    return evacuated_agents;
}

void BuildingSystem::reopen_building(std::uint32_t building_id) {
    auto it = occupancy_states_.find(building_id);
    const auto* building = building_registry_.get(building_id);
    if (it == occupancy_states_.end() || !building) return;
    it->second.evacuated = false;
    it->second.is_open = building_open(*building, it->second);
}

} // namespace npc_crowd::simulation::systems
