#pragma once

#include "core/types.hpp"
#include "world/map.hpp"
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace npc_crowd::simulation::systems {

/// Estado de semáforo por tick
struct TrafficLightState {
    std::uint32_t light_id = 0;
    std::uint32_t node_id = 0;
    enum class Phase : std::uint8_t { NS_Green, NS_Yellow, EW_Green, EW_Yellow, All_Red };
    Phase current_phase = Phase::NS_Green;
    float phase_timer = 0.f;       // segundos dentro del ciclo
    float cycle_length = 0.f;      // segundos
    bool pedestrian_request = false;
    float pedestrian_cross_time = 0.f;
    float time_until_change = 0.f; // segundos hasta el siguiente cambio de fase
};

/// Resultado de movimiento de vehículo
enum class VehicleMovementResult {
    Success = 0,
    WaitingAtLight = 1,
    WaitingForGap = 2,
    Blocked = 3,
    OffRoute = 4,
    ArrivedAtDestination = 5
};

/// Configuración de semáforo por intersección
struct TrafficLightConfig {
    std::uint32_t node_id = 0;    // Nodo donde está
    float ns_green = 30.f;        // Duración verde N-S (segundos)
    float ns_yellow = 3.f;        // Amarillo N-S
    float ew_green = 25.f;        // Verde E-W
    float ew_yellow = 3.f;        // Amarillo E-W
    float all_red = 2.f;          // Rojo completo
    float pedestrian_cross_time = 10.f; // Tiempo cruce peatón
    std::vector<std::uint32_t> controlled_edges; // Edges que controla
    std::uint32_t min_phase_time = 5; // Mínimo segundos por fase

    float cycle() const { return ns_green + ns_yellow + ew_green + ew_yellow + all_red; }
};

/// Vehículo controlado por el sistema de tráfico
struct SystemVehicle {
    std::uint32_t id = 0;
    enum class Type { Car, Bus, Emergency, Delivery, Taxi, Bike } type = Type::Car;
    Vec2 position{};
    Vec2 velocity{};
    Vec2 destination{};

    // Estado de ruta
    std::uint32_t current_edge = 0;    // 0 = sin edge
    std::uint32_t edge_from_node = 0;  // Nodo desde el que recorre el edge actual
    float progress_on_edge = 0.f;      // 0-1 en el edge actual
    std::vector<std::uint32_t> path_edges; // Edges pendientes (el primero es current_edge)
    std::uint32_t destination_node = 0;
    float target_speed = 13.9f;        // m/s (50 km/h)

    // Estado semáforo
    bool waiting_at_light = false;
    std::uint32_t current_light = 0;
    float light_timer = 0.f;           // segundos esperando en el semáforo actual

    // Para buses
    std::uint32_t route_id = 0;
    std::uint32_t next_stop_index = 0;
    float dwell_timer = 0.f;
    bool at_stop = false;
    std::uint32_t stop_node = 0;

    // Estado
    bool is_active = true;
    VehicleMovementResult last_result = VehicleMovementResult::Success;
    std::uint32_t trips_completed = 0;
    float fuel = 1.f;
    float pollution = 0.f;
};

/// Configuración de parámetros de tráfico
struct TrafficSystemConfig {
    bool enable_traffic_lights = true;
    float light_cycle_speed = 1.f;     // 1.0 = tiempo real, 2.0 = doble velocidad
    float pedestrian_priority = 0.8f;  // Prioridad peatón vs vehículos
    float emergency_vehicle_priority = 2.f; // > 1: los vehículos de emergencia no se detienen en rojo
    float bus_priority = 1.2f;         // Prioridad autobús
    float default_speed_limit = 13.9f; // m/s (50 km/h base)
    float speed_penalty_per_pedestrian = 0.05f; // Fracción de velocidad perdida por peatón en el tramo
    float rush_hour_factor = 1.3f;     // Divisor de velocidad en hora punta (congestión)
    float vehicles_per_resident = 0.2f;
    float stuck_light_timeout = 120.f; // segundos máximos de espera antes de considerar el semáforo averiado
};

/// Sistema de gestión de tráfico sobre un world::NavGraph
class TrafficSystem {
public:
    TrafficSystem(const TrafficSystemConfig& config, const world::NavGraph& graph);

    /// Crea semáforos y la flota de vehículos (población * vehicles_per_resident) con rutas aleatorias deterministas.
    void initialize(std::uint32_t population, std::uint64_t seed = 42);

    /// Avanza el tráfico dt segundos. pedestrian_density_by_edge es opcional (peatones por edge id).
    void update(float dt, float world_hour, int week_day,
                const std::unordered_map<std::uint32_t, float>& pedestrian_density_by_edge = {});

    // Consultas
    TrafficLightState get_light_state(std::uint32_t light_id) const;
    std::vector<TrafficLightState> get_all_light_states() const;
    float get_speed_limit(std::uint32_t edge_id, world::RoadType road_type, float pedestrian_density = 0.f) const;
    bool is_gridlock(std::uint32_t edge_id) const;
    std::vector<std::uint32_t> get_congested_edges(float threshold_ratio = 0.8f) const;
    std::vector<std::uint32_t> get_gridlocked_nodes() const;
    const std::vector<SystemVehicle>& vehicles() const { return system_vehicles_; }
    bool is_edge_blocked(std::uint32_t edge_id) const { return blocked_edges_.count(edge_id) > 0; }

    // Eventos de tráfico
    void inject_traffic_block(Vec2 position, float radius, float duration_seconds);
    void inject_event(EventType type, Vec2 position, float radius);

    // Configuración dinámica
    void set_light_config(std::uint32_t light_id, const TrafficLightConfig& config);
    void set_config(const TrafficSystemConfig& config) { config_ = config; }

    // Ruta más corta (Dijkstra) evitando edges bloqueados. Devuelve edges en orden; vacío si no hay ruta.
    std::vector<std::uint32_t> find_route(std::uint32_t from_node, std::uint32_t to_node) const;

    // Estadísticas del último update
    struct Stats {
        std::size_t total_vehicles = 0;
        std::size_t vehicles_moving = 0;
        std::size_t vehicles_waiting = 0;  // en semáforo
        std::size_t vehicles_blocked = 0;  // sin ruta por cortes
        std::size_t vehicles_stopped = 0;
        std::size_t trips_completed = 0;   // acumulado
        float total_wait_time = 0.f;       // segundos acumulados en semáforos
        float avg_speed = 0.f;             // m/s de los vehículos en movimiento
        std::size_t traffic_light_stops = 0;
        std::size_t pedestrian_slowdowns = 0;
    } stats;

private:
    void initialize_lights();
    void update_lights(float dt);
    VehicleMovementResult update_vehicle(SystemVehicle& veh, float dt, float speed_factor,
                                         const std::unordered_map<std::uint32_t, float>& pedestrian_density);
    bool can_pass_light(const SystemVehicle& veh, std::uint32_t node_id) const;
    bool assign_new_trip(SystemVehicle& veh, std::uint32_t from_node);
    bool start_next_edge(SystemVehicle& veh);
    std::uint32_t other_end(const world::NavEdge& edge, std::uint32_t node) const;
    float edge_length(const world::NavEdge& edge) const;
    Vec2 node_pos(std::uint32_t node_id) const;
    float rush_hour_divisor() const;

    TrafficSystemConfig config_;
    const world::NavGraph& graph_;
    std::mt19937_64 rng_{42};

    std::unordered_map<std::uint32_t, TrafficLightState> lights_;
    std::unordered_map<std::uint32_t, TrafficLightConfig> light_configs_;
    std::unordered_map<std::uint32_t, std::uint32_t> node_light_; // node_id -> light_id

    std::vector<SystemVehicle> system_vehicles_;
    std::vector<std::uint32_t> node_list_; // ids de nodo ordenados (muestreo determinista)
    std::unordered_map<std::uint32_t, float> blocked_edges_; // edge_id -> segundos restantes

    float current_world_hour_ = 6.f;
    int current_week_day_ = 0;
};

} // namespace npc_crowd::simulation::systems
