#pragma once

#include "core/types.hpp"
#include "world/building.hpp"
#include "world/map.hpp"
#include "world/zone.hpp"
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace npc_crowd::world {

/// Tipo de acera
enum class SidewalkType : std::uint8_t {
    Standard = 0,           // Acera estándar
    Wide = 1,               // Acera ancha (centro comercial)
    PedestrianStreet = 2,   // Calle peatonal completa
    Plaza = 3,              // Plaza pública
    ParkPath = 4,           // Sendero en parque
    Bridge = 5,             // Puente peatonal
    Underpass = 6,          // Paso subterráneo
    Shared = 7              // Compartida bici/peatón
};

/// Estado de la acera
enum class SidewalkCondition : std::uint8_t {
    Excellent = 0,
    Good = 1,
    Fair = 2,
    Poor = 3,
    UnderConstruction = 4,
    Blocked = 5
};

/// Segmento de acera (edge en el grafo peatonal)
struct SidewalkSegment {
    std::uint32_t id;
    std::uint32_t from_node;      // Nodo peatonal inicio
    std::uint32_t to_node;        // Nodo peatonal fin
    SidewalkType type = SidewalkType::Standard;
    float width = 2.5f;           // metros
    float length = 0.f;           // metros (calculado)
    float walk_speed = 1.4f;      // m/s velocidad base peatón
    SidewalkCondition condition = SidewalkCondition::Good;
    
    // Accesibilidad
    bool has_curb_cuts = true;    // Rebajes en cruces
    bool has_tactile_paving = false; // Pavimento táctil
    bool is_wheelchair_accessible = true;
    float incline = 0.f;          // Pendiente -1 a 1
    
    // Conectividad
    std::uint32_t parallel_road_edge = 0; // Edge de carretera paralela (si hay)
    std::vector<std::uint32_t> crossing_ids; // Cruces peatonales en este segmento
    std::vector<std::uint32_t> building_entrances; // Entradas a edificios
    
    // Estado dinámico
    float current_density = 0.f;  // personas/m2
    float comfort_level = 1.f;    // 0-1 (sombra, ruido, limpieza)
    bool is_blocked = false;
    std::uint32_t blockage_reason = 0; // 0=none, 1=construction, 2=event, 3=vehicle
    
    // Métodos
    float effective_walk_speed() const {
        float speed = walk_speed;
        speed *= condition == SidewalkCondition::Excellent ? 1.1f : 
                 condition == SidewalkCondition::Good ? 1.0f :
                 condition == SidewalkCondition::Fair ? 0.85f :
                 condition == SidewalkCondition::Poor ? 0.6f : 0.1f;
        speed *= (1.f - incline * 0.3f); // Penalizar subidas
        speed *= (1.f - current_density * 0.5f); // Penalizar densidad
        return std::max(0.1f, speed);
    }
    
    float capacity() const { return width * length * 2.f; } // ~2 personas/m2 cómodo
    float occupancy_ratio() const { return capacity() > 0 ? current_density / (2.f) : 0.f; }
};

/// Nodo peatonal (intersección de aceras, entrada edificio, cruce)
struct PedestrianNode {
    std::uint32_t id;
    Vec2 position;
    std::vector<std::uint32_t> connected_segments; // SidewalkSegment IDs
    std::vector<std::uint32_t> building_entrances; // Building entrance IDs
    std::vector<std::uint32_t> crossing_ids;       // PedestrianCrossing IDs
    std::vector<std::uint32_t> transit_stop_ids;   // TransitStop IDs
    std::uint32_t zone_id = 0;
    
    // Tipo de nodo
    enum class NodeType : std::uint8_t {
        Intersection = 0,
        BuildingEntrance = 1,
        Crossing = 2,
        TransitStop = 3,
        POI = 4,              // Punto de interés (banco, fuente, etc.)
        DeadEnd = 5
    };
    NodeType type = NodeType::Intersection;
    
    // Servicios en el nodo
    bool has_bench = false;
    bool has_shelter = false;
    bool has_lighting = true;
    bool has_wifi = false;
    bool has_charging = false;
    
    // Estado dinámico
    float current_crowd = 0.f;      // Personas en el nodo
    float wait_time = 0.f;          // Tiempo medio espera (cruces)
};

/// Red peatonal completa (grafo separado pero vinculado al vial)
struct PedestrianNetwork {
    std::unordered_map<std::uint32_t, PedestrianNode> nodes;
    std::unordered_map<std::uint32_t, SidewalkSegment> segments;
    std::uint32_t next_node_id = 1;
    std::uint32_t next_segment_id = 1;
    
    // Índice espacial para queries rápidas
    std::vector<Vec2> node_positions;
    std::vector<std::uint32_t> node_ids;
    
    // Métodos de consulta
    std::optional<std::uint32_t> find_nearest_node(Vec2 pos, float max_dist = 30.f) const;
    std::vector<std::uint32_t> find_nodes_in_radius(Vec2 pos, float radius) const;
    std::vector<std::uint32_t> find_segments_near(Vec2 pos, float radius) const;
    
    // Pathfinding peatonal (A* en grafo peatonal)
    std::vector<std::uint32_t> find_path(std::uint32_t start, std::uint32_t goal) const;
    float heuristic(std::uint32_t a, std::uint32_t b) const;
    float path_length(const std::vector<std::uint32_t>& node_path) const;
    float path_time(const std::vector<std::uint32_t>& node_path, float walk_speed = 1.4f) const;
    
    // Actualización densidad (llamado cada tick)
    void update_densities(const std::vector<Vec2>& agent_positions, float agent_radius = 0.5f);
    
    // Serialización
    void save_binary(const std::string& path) const;
    static PedestrianNetwork load_binary(const std::string& path);
};

/// Generador de red peatonal a partir del grafo vial
struct SidewalkGenerator {
    struct Params {
        float default_width = 2.5f;
        float wide_width = 5.f;
        float plaza_width = 10.f;
        float min_width = 1.5f;      // Mínimo legal
        bool generate_crossings = true;
        bool generate_tactile_paving = true;
        float crossing_spacing = 80.f; // Distancia entre cruces
        bool connect_all_buildings = true;
    };
    
    static PedestrianNetwork generate_from_road_graph(const NavGraph& road_graph, const Params& params);
    static PedestrianNetwork generate_grid(const Params& params, float world_w, float world_h);
    
private:
    static void create_sidewalk_parallel_to_roads(PedestrianNetwork& ped_net, const NavGraph& road_graph, const Params& params);
    static void create_crossings(PedestrianNetwork& ped_net, const NavGraph& road_graph, const Params& params);
    static void connect_buildings(PedestrianNetwork& ped_net, const BuildingRegistry& buildings, const NavGraph& road_graph);
    static void add_plazas_and_parks(PedestrianNetwork& ped_net, const ZoneRegistry& zones);
    static void validate_connectivity(PedestrianNetwork& ped_net);
};

} // namespace npc_crowd::world
