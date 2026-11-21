#pragma once

#include "core/types.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

namespace npc_crowd::world {

// Forward declarations
struct Building;
struct Zone;
struct Sidewalk;

/// Tipos de carretera
enum class RoadType : std::uint8_t {
    Highway = 0,      // Autopista - solo vehículos, alta velocidad
    Arterial = 1,     // Vía principal - vehículos + buses, velocidad media
    Collector = 2,    // Colectora - mixto, velocidad media-baja
    Local = 3,        // Local - residencial, velocidad baja
    Pedestrian = 4    // Solo peatones - plaza, calle peatonal
};

/// Tipos de cruce
enum class CrossingType : std::uint8_t {
    None = 0,
    Zebra = 1,           // Paso de cebra
    TrafficLight = 2,    // Semáforo peatonal
    PedestrianBridge = 3,// Puente peatonal
    Underpass = 4        // Subterráneo
};

/// Nodo del grafo de navegación (intersección, esquina, punto de interés)
struct NavNode {
    std::uint32_t id;
    Vec2 position;
    std::vector<std::uint32_t> edges;  // IDs de edges conectados
    std::vector<std::uint32_t> buildings; // Edificios accesibles desde este nodo
    std::vector<std::uint32_t> sidewalks; // Aceras conectadas
    std::vector<std::uint32_t> crossings; // Cruces peatonales
    std::uint32_t zone_id;             // Zona a la que pertenece
    bool is_intersection = true;
    bool is_building_entrance = false;
    bool is_transit_stop = false;
    bool is_crossing = false;
};

/// Edge del grafo (tramo de calle/acera)
struct NavEdge {
    std::uint32_t id;
    std::uint32_t from_node;
    std::uint32_t to_node;
    RoadType road_type = RoadType::Local;
    float length = 0.f;
    float max_speed = 5.f;      // m/s para vehículos
    float walk_speed = 1.4f;    // m/s para peatones
    bool has_sidewalk = true;
    bool is_bidirectional = true;
    std::uint32_t sidewalk_edge_id = 0; // Edge paralelo de acera (si existe)
    std::vector<std::uint32_t> lanes;   // IDs de carriles (para vehículos)
    std::uint32_t zone_id = 0;
};

/// Carriles para vehículos
struct Lane {
    std::uint32_t id;
    std::uint32_t edge_id;
    int index = 0;              // 0 = carril derecho, 1 = siguiente, etc.
    bool is_bus_lane = false;
    bool is_bike_lane = false;
    float width = 3.5f;
    std::vector<Vec2> waypoints; // Puntos de control para curvatura
};

/// Semáforo
struct TrafficLight {
    std::uint32_t id;
    std::uint32_t node_id;      // Nodo donde está el semáforo
    enum class Phase : std::uint8_t { NS_Green, NS_Yellow, EW_Green, EW_Yellow, All_Red };
    Phase current_phase = Phase::NS_Green;
    float phase_timer = 0.f;
    float ns_green_duration = 30.f;
    float ns_yellow_duration = 3.f;
    float ew_green_duration = 25.f;
    float ew_yellow_duration = 3.f;
    float all_red_duration = 2.f;
    std::vector<std::uint32_t> controlled_edges; // Edges controlados
    bool pedestrian_request = false;
    float pedestrian_cross_time = 10.f;
};

/// Paso de cebra / cruce peatonal
struct PedestrianCrossing {
    std::uint32_t id;
    std::uint32_t node_id;      // Nodo donde está el cruce
    CrossingType type = CrossingType::Zebra;
    std::uint32_t traffic_light_id = 0; // Semáforo asociado (si hay)
    Vec2 start_pos, end_pos;
    float width = 3.f;
    bool has_tactile_paving = true;
    std::vector<std::uint32_t> connected_sidewalks; // Aceras que conecta
};

/// Parada de transporte público
struct TransitStop {
    std::uint32_t id;
    std::uint32_t node_id;
    Vec2 position;
    std::string name;
    std::vector<std::uint32_t> routes; // IDs de rutas que paran aquí
    float shelter_capacity = 20.f;    // Personas que caben en la marquesina
    bool has_bench = true;
    bool has_real_time_info = false;
};

/// Ruta de bus
struct BusRoute {
    std::uint32_t id;
    std::string name;
    std::string number;         // Ej: "42", "C1", "N5"
    std::vector<std::uint32_t> stop_ids; // Orden de paradas
    std::vector<float> stop_times;       // Tiempo estimado entre paradas (segundos)
    float headway = 600.f;      // Frecuencia en segundos (10 min = 600)
    float speed_factor = 0.8f;  // Factor de velocidad vs coche privado
    bool is_circular = true;
    bool is_night_route = false;
};

/// Vehículo en simulación
struct Vehicle {
    std::uint32_t id;
    enum class Type : std::uint8_t { Car, Bus, Tram, Emergency, Delivery, Taxi, Bicycle };
    Type type = Type::Car;
    Vec2 position;
    Vec2 velocity;
    std::uint32_t current_edge = 0;
    std::uint32_t current_lane = 0;
    float progress_on_edge = 0.f; // 0-1
    float target_speed = 13.9f;   // m/s (50 km/h)
    float max_accel = 2.5f;
    float max_decel = 4.5f;
    std::uint32_t route_edge_index = 0;
    std::vector<std::uint32_t> path_edges; // Ruta calculada
    std::uint32_t destination_node = 0;
    // Para buses
    std::uint32_t route_id = 0;
    std::uint32_t next_stop_index = 0;
    float dwell_timer = 0.f;
    bool at_stop = false;
    // Estado
    bool waiting_at_light = false;
    std::uint32_t light_id = 0;
};

/// Grafo de navegación completo
struct NavGraph {
    std::unordered_map<std::uint32_t, NavNode> nodes;
    std::unordered_map<std::uint32_t, NavEdge> edges;
    std::unordered_map<std::uint32_t, Lane> lanes;
    std::unordered_map<std::uint32_t, TrafficLight> traffic_lights;
    std::unordered_map<std::uint32_t, PedestrianCrossing> crossings;
    std::unordered_map<std::uint32_t, TransitStop> transit_stops;
    std::unordered_map<std::uint32_t, BusRoute> bus_routes;
    std::vector<Vehicle> vehicles;
    
    // IDs auto-incrementales
    std::uint32_t next_node_id = 1;
    std::uint32_t next_edge_id = 1;
    std::uint32_t next_lane_id = 1;
    std::uint32_t next_light_id = 1;
    std::uint32_t next_crossing_id = 1;
    std::uint32_t next_stop_id = 1;
    std::uint32_t next_route_id = 1;
    std::uint32_t next_vehicle_id = 1;
    
    // Spatial index para queries rápidas
    std::vector<Vec2> node_positions; // Para spatial grid
    std::vector<std::uint32_t> node_ids;
    
    // Métodos de consulta
    std::optional<std::uint32_t> find_nearest_node(Vec2 pos, float max_dist = 50.f) const;
    std::optional<std::uint32_t> find_nearest_edge(Vec2 pos, float max_dist = 20.f) const;
    std::vector<std::uint32_t> find_nodes_in_radius(Vec2 pos, float radius) const;
    std::vector<std::uint32_t> find_crossings_near(Vec2 pos, float radius) const;
    std::vector<std::uint32_t> find_transit_stops_near(Vec2 pos, float radius) const;
    
    // Pathfinding
    std::vector<std::uint32_t> find_path_nodes(std::uint32_t start, std::uint32_t goal, bool pedestrian = false) const;
    std::vector<std::uint32_t> find_path_edges(std::uint32_t start, std::uint32_t goal, bool pedestrian = false) const;
    float heuristic(std::uint32_t a, std::uint32_t b) const;
    
    // Actualización de semáforos
    void update_traffic_lights(float dt);
    
    // Serialización
    void save_binary(const std::string& path) const;
    static NavGraph load_binary(const std::string& path);
};

/// Generador de mapas procedimentales
struct MapGenerator {
    struct Params {
        std::uint32_t seed = 42;
        float world_width = 2000.f;
        float world_height = 1500.f;
        float block_size = 120.f;       // Tamaño de manzana
        float street_width = 12.f;      // Ancho calle
        float sidewalk_width = 3.f;     // Ancho acera
        float arterial_spacing = 480.f; // Espaciado vías principales
        float highway_probability = 0.05f;
        float pedestrian_street_probability = 0.1f;
        std::uint32_t min_buildings_per_block = 2;
        std::uint32_t max_buildings_per_block = 8;
        float transit_coverage = 0.7f;  // Fracción de Área con transporte
    };
    
    static NavGraph generate(const Params& params);
    static NavGraph generate_grid_city(const Params& params);
    static NavGraph generate_organic_city(const Params& params);
    
private:
    static void create_grid_nodes(NavGraph& graph, const Params& params);
    static void create_grid_edges(NavGraph& graph, const Params& params);
    static void add_arterial_roads(NavGraph& graph, const Params& params);
    static void add_highways(NavGraph& graph, const Params& params);
    static void add_sidewalks(NavGraph& graph, const Params& params);
    static void add_crossings(NavGraph& graph, const Params& params);
    static void add_traffic_lights(NavGraph& graph, const Params& params);
    static void add_transit_network(NavGraph& graph, const Params& params);
    static void place_buildings(NavGraph& graph, const Params& params);
    static void place_zones(NavGraph& graph, const Params& params);
    static void validate_connectivity(NavGraph& graph);
};

} // namespace npc_crowd::world
