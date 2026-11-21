#pragma once

#include "core/types.hpp"
#include "world/building.hpp"
#include "world/map.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace npc_crowd::world {

/// Tipos de zona/distrito urbano
enum class ZoneType : std::uint8_t {
    // Residencial
    ResidentialLow = 0,      // Baja densidad (chalets, casas)
    ResidentialMedium = 1,   // Media densidad (bloques 4-8 plantas)
    ResidentialHigh = 2,     // Alta densidad (rascacielos)
    ResidentialMixed = 3,    // Mixto vivienda/comercio planta baja
    
    // Comercial
    CommercialRetail = 10,   // Comercio minorista
    CommercialOffice = 11,   // Distrito de oficinas
    CommercialMixed = 12,    // Mixto oficinas/comercio
    FinancialDistrict = 13,  // Distrito financiero
    
    // Industrial
    IndustrialLight = 20,    // Industrial ligera
    IndustrialHeavy = 21,    // Industrial pesada
    Logistics = 22,          // Logística / distribución
    TechPark = 23,           // Parque tecnológico
    
    // Institucional / Público
    CivicCenter = 30,        // Centro cívico / admin
    Educational = 31,        // Campus educativo
    Medical = 32,            // Distrito sanitario
    Government = 33,         // Administrativo público
    Security = 34,           // Seguridad / emergencias
    
    // Cultura / Ocio
    Cultural = 40,           // Cultural / museos
    Entertainment = 41,      // Ocio nocturno / teatros
    Sports = 42,             // Deportivo
    Tourism = 43,            // Turístico
    
    // Verde / Espacio abierto
    Park = 50,               // Parque urbano
    NatureReserve = 51,      // Reserva natural
    Greenbelt = 52,          // Cinturón verde
    Waterfront = 53,         // Frente marítimo / ribereño
    
    // Transporte
    TransportHub = 60,       // Nudo transporte
    AirportZone = 61,        // Zona aeroportuaria
    Port = 62,               // Puerto
    
    // Especial
    Historic = 70,           // Centro histórico
    Slum = 71,               // Asentamiento informal
    Construction = 72,       // En construcción
    Military = 73,           // Militar
    University = 74          // Campus universitario
};

/// Configuración de zona (reglas, densidades, atracción)
struct ZoneConfig {
    ZoneType type;
    std::string name;
    
    // Densidades objetivo (por km2 o por hectárea)
    float target_population_density = 5000.f;  // hab/km2
    float target_job_density = 3000.f;         // empleos/km2
    float target_building_density = 0.4f;      // ratio huella
    
    // Distribución de tipos de edificio (pesos relativos)
    std::unordered_map<BuildingType, float> building_weights;
    
    // Distribución de perfiles de rutina (pesos)
    std::unordered_map<RoutineProfile, float> routine_weights;
    
    // Reglas de circulación
    float max_vehicle_speed = 50.f;        // km/h
    float pedestrian_priority = 1.f;       // 0-1, prioridad peatón
    bool cars_allowed = true;
    bool trucks_allowed = false;
    bool buses_allowed = true;
    bool bicycles_allowed = true;
    bool parking_allowed = true;
    float parking_cost_per_hour = 0.f;
    
    // Atracción para actividades
    float live_attraction = 1.f;
    float work_attraction = 1.f;
    float shop_attraction = 1.f;
    float leisure_attraction = 1.f;
    float study_attraction = 1.f;
    float medical_attraction = 1.f;
    
    // Factores socioeconómicos
    float avg_income = 25000.f;           // €/año
    float avg_rent = 800.f;               // €/mes
    float crime_rate = 0.01f;             // 0-1
    float pollution = 0.1f;               // 0-1
    float noise_level = 0.2f;             // 0-1
    float walkability = 0.7f;             // 0-1
    float transit_access = 0.5f;          // 0-1
    
    // Servicios mínimos requeridos (por 1000 hab)
    std::unordered_map<BuildingType, float> required_services;
    
    static const std::unordered_map<ZoneType, ZoneConfig>& all_configs();
    static const ZoneConfig& get(ZoneType type);
};

/// Distrito/barrio concreto en el mundo
struct Zone {
    std::uint32_t id;
    ZoneType type = ZoneType::ResidentialMedium;
    std::string name;
    
    // Geometría (polígono simple)
    std::vector<Vec2> polygon;            // Vértices en sentido horario
    Vec2 centroid;                        // Centro calculado
    float area = 0.f;                     // m2
    
    // Contenido
    std::unordered_set<std::uint32_t> building_ids;
    std::unordered_set<std::uint32_t> node_ids;      // Nodos NavGraph dentro
    std::unordered_set<std::uint32_t> edge_ids;      // Edges NavGraph dentro
    
    // Población y empleo
    std::uint32_t resident_count = 0;
    std::uint32_t worker_count = 0;
    std::uint32_t visitor_count = 0;      // Visitantes actuales
    
    // Estadísticas dinámicas
    float current_population_density = 0.f;
    float current_job_density = 0.f;
    float avg_stress = 0.f;
    float avg_income = 0.f;
    float crime_incidents = 0.f;
    
    // Configuración (override de ZoneConfig)
    ZoneConfig config_override;
    bool use_override = false;
    
    // Métodos
    bool contains(Vec2 pos) const;        // Point-in-polygon
    bool contains_building(std::uint32_t building_id) const;
    void add_building(std::uint32_t building_id);
    void remove_building(std::uint32_t building_id);
    void recalculate_stats(const BuildingRegistry& buildings);
    Vec2 get_random_point() const;
    std::vector<Vec2> get_entrance_points() const; // Puntos de conexión con otras zonas
};

/// Registro de todas las zonas
struct ZoneRegistry {
    std::unordered_map<std::uint32_t, Zone> zones;
    std::uint32_t next_id = 1;
    
    std::uint32_t add(Zone&& zone) {
        zone.id = next_id++;
        auto id = zone.id;
        zones.emplace(id, std::move(zone));
        return id;
    }
    
    Zone* get(std::uint32_t id) {
        auto it = zones.find(id);
        return it != zones.end() ? &it->second : nullptr;
    }
    
    const Zone* get(std::uint32_t id) const {
        auto it = zones.find(id);
        return it != zones.end() ? &it->second : nullptr;
    }
    
    void remove(std::uint32_t id) { zones.erase(id); }
    
    std::vector<std::uint32_t> find_by_type(ZoneType type) const;
    std::vector<std::uint32_t> find_containing(Vec2 pos) const;
    std::vector<std::uint32_t> find_near(Vec2 pos, float radius) const;
    std::optional<std::uint32_t> find_at(Vec2 pos) const; // Primera que contenga el punto
    
    // Adyacencia
    std::vector<std::uint32_t> get_adjacent_zones(std::uint32_t zone_id) const;
    void build_adjacency_graph();
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> adjacency;
};

/// Generador de zoning procedimental
struct ZoneGenerator {
    struct Params {
        std::uint32_t seed = 42;
        float world_width = 2000.f;
        float world_height = 1500.f;
        float zone_size = 400.f;          // Tamaño aproximado de zona
        float transition_width = 50.f;    // Ancho de transición entre zonas
        std::vector<ZoneType> allowed_types; // Vacío = todos
        float mixed_use_probability = 0.3f;
    };
    
    static std::vector<Zone> generate_zoning(const Params& params, const NavGraph& graph);
    static std::vector<Zone> generate_concentric(const Params& params);     // Centro -> periferia
    static std::vector<Zone> generate_sector(const Params& params);         // Sectores radiales
    static std::vector<Zone> generate_grid_zoning(const Params& params);    // Cuadrícula
    static std::vector<Zone> generate_organic(const Params& params);        // Orgánico (ruido)
    
private:
    static void assign_zone_types(std::vector<Zone>& zones, const Params& params);
    static void smooth_boundaries(std::vector<Zone>& zones, float transition_width);
    static void calculate_polygons(std::vector<Zone>& zones, const NavGraph& graph);
};

} // namespace npc_crowd::world
