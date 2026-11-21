#pragma once

#include "core/types.hpp"
#include "world/building.hpp"
#include "world/zone.hpp"
#include "world/map.hpp"
#include <initializer_list>
#include <unordered_map>
#include <vector>

namespace npc_crowd::simulation::systems {

/// Estado de ocupación de edificio por tick
struct BuildingOccupancyState {
    std::uint32_t building_id = 0;
    std::uint32_t current_occupancy = 0;
    std::uint32_t max_capacity = 0;
    float occupancy_ratio = 0.f;
    bool is_open = false;
    bool evacuated = false;       // Cerrado por un evento hasta que se reabra
    float time_until_open = 0.f;  // horas (0 si abierto ahora)
    float time_until_close = 0.f; // horas
    std::vector<std::uint32_t> agents_inside;

    // Zonas interiores
    std::unordered_map<std::uint32_t, std::uint32_t> zone_occupancy; // zone_id -> count

    // Actividad
    bool has_pending_jobs = false;
    std::uint32_t pending_jobs_count = 0;

    // Estado dinámico
    float condition = 1.f;           // 0-1 estado del edificio
    float maintenance_rate = 0.001f; // Degradación por unidad de tiempo sin mantenimiento
};

/// Resultado de operación de edificio
enum class BuildingOperationResult {
    Success = 0,
    AtCapacity = 1,       // Llegó a capacidad máxima
    Closed = 2,           // Fuera de horario o evacuado
    Restricted = 3,       // Zona restringida
    AgentNotFound = 4,    // Agente no pertenece a este edificio
    InvalidZone = 5,      // Zona interior inválida
    BuildingNotFound = 6  // El edificio no existe en el registro
};

/// Sistema de gestión de edificios y ocupación
class BuildingSystem {
public:
    BuildingSystem(const world::BuildingRegistry& registry, const world::ZoneRegistry& zone_registry);

    /// Crea el estado de ocupación de cada edificio del registro.
    void initialize();

    /// Reloj de simulación usado para comprobar horarios (hora del día, día de la semana 0=lunes, día del año).
    void set_clock(float world_hour, int week_day, int day_of_year);

    // Operaciones de entrada/salida. zone_id 0 = sin zona concreta (vestíbulo).
    BuildingOperationResult enter_building(std::uint32_t agent_id, std::uint32_t target_building_id,
                                           std::uint32_t target_zone_id = 0, bool authorized = false);
    BuildingOperationResult exit_building(std::uint32_t agent_id, std::uint32_t target_building_id);

    // Consultas
    BuildingOccupancyState get_occupancy(std::uint32_t building_id) const;
    std::uint32_t get_building_for_agent(std::uint32_t agent_id) const; // 0 = en la calle
    std::uint32_t get_zone_for_agent(std::uint32_t agent_id) const;
    const world::Building* get_building_definition(std::uint32_t building_id) const;
    const world::InteriorZone* get_zone_definition(std::uint32_t building_id, std::uint32_t zone_id) const;

    // Por tipo de edificio (solo edificios abiertos y con plazas libres)
    std::vector<std::uint32_t> find_open_of_types(std::initializer_list<world::BuildingType> types,
                                                  std::uint32_t min_capacity = 1) const;
    std::vector<std::uint32_t> find_open_hospitals(std::uint32_t min_capacity = 1) const;
    std::vector<std::uint32_t> find_open_schools(std::uint32_t min_capacity = 1) const;
    std::vector<std::uint32_t> find_open_offices(std::uint32_t min_capacity = 1) const;
    std::vector<std::uint32_t> find_open_police_stations(std::uint32_t min_capacity = 1) const;
    std::vector<std::uint32_t> find_open_fire_stations(std::uint32_t min_capacity = 1) const;
    std::vector<std::uint32_t> find_open_universities(std::uint32_t min_capacity = 1) const;

    // Por zona urbana (world::Zone::building_ids)
    std::vector<std::uint32_t> find_buildings_in_zone(std::uint32_t zone_id) const;
    std::vector<std::uint32_t> find_open_buildings_in_zone(std::uint32_t zone_id) const;

    // Mantenimiento
    void update_maintenance(float dt);
    void repair_building(std::uint32_t building_id);
    void set_building_condition(std::uint32_t building_id, float condition);

    // Eventos. Devuelve los agentes desalojados (para que la simulación los saque a la calle).
    std::vector<std::uint32_t> inject_event(EventType type, Vec2 position, float radius);
    void reopen_building(std::uint32_t building_id);

    // Estadísticas globales
    struct SystemStats {
        std::size_t total_buildings = 0;
        std::size_t total_capacity = 0;
        std::size_t total_occupancy = 0;
        float avg_occupancy_ratio = 0.f;
        std::size_t total_enter_attempts = 0;
        std::size_t total_enter_successes = 0;
        std::size_t total_exit_attempts = 0;
        std::size_t total_exit_successes = 0;
        std::unordered_map<world::BuildingType, std::uint32_t> capacity_by_type;
        std::unordered_map<world::BuildingType, std::uint32_t> occupancy_by_type;
    } stats;

private:
    bool building_open(const world::Building& building, const BuildingOccupancyState& state) const;
    void refresh_ratio(BuildingOccupancyState& state) const;

    const world::BuildingRegistry& building_registry_;
    const world::ZoneRegistry& zone_registry_;

    std::unordered_map<std::uint32_t, BuildingOccupancyState> occupancy_states_;
    std::unordered_map<std::uint32_t, std::uint32_t> agent_building_; // agent_id -> building_id
    std::unordered_map<std::uint32_t, std::uint32_t> agent_zone_;     // agent_id -> zone_id

    float world_hour_ = 6.f;
    int week_day_ = 0;
    int day_of_year_ = 1;
};

} // namespace npc_crowd::simulation::systems
