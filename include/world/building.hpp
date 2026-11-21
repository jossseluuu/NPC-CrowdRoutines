#pragma once

#include "core/types.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <optional>

namespace npc_crowd::world {

/// Tipos de edificio
enum class BuildingType : std::uint8_t {
    // Residencial
    House = 0,              // Casa unifamiliar
    ApartmentBlock = 1,     // Bloque de pisos
    HighRise = 2,           // Rascacielos residencial
    GatedCommunity = 3,     // Urbanización cerrada
    
    // Comercial / Oficinas
    OfficeSmall = 10,       // Oficina pequeña (<50 emp)
    OfficeMedium = 11,      // Oficina mediana (50-200 emp)
    OfficeLarge = 12,       // Oficina grande (200-1000 emp)
    CorporateHQ = 13,       // Sede corporativa (>1000 emp)
    Coworking = 14,         // Espacio coworking
    
    // Comercial / Retail
    ConvenienceStore = 20,  // Tienda conveniencia
    Supermarket = 21,       // Supermercado
    Mall = 22,              // Centro comercial
    DepartmentStore = 23,   // Grandes almacenes
    StreetRetail = 24,      // Comercio a pie de calle
    Market = 25,            // Mercado tradicional
    
    // Industrial / Logística
    Factory = 30,           // Fábrica
    Warehouse = 31,         // Almacén logístico
    DistributionCenter = 32,// Centro distribución
    Workshop = 33,          // Taller artesanal
    DataCenter = 34,        // Centro de datos
    
    // Sanidad
    Hospital = 40,          // Hospital general
    Clinic = 41,            // Clínica / ambulatorio
    Pharmacy = 42,          // Farmacia
    NursingHome = 43,       // Residencia ancianos
    DentalClinic = 44,      // Clínica dental
    VetClinic = 45,         // Veterinario
    
    // Educación
    Kindergarten = 50,      // Guardería
    PrimarySchool = 51,     // Colegio primaria
    SecondarySchool = 52,   // Instituto secundaria
    HighSchool = 53,        // Bachillerato
    University = 54,        // Universidad
    VocationalSchool = 55,  // Formación profesional
    Library = 56,           // Biblioteca
    
    // Seguridad / Emergencias
    PoliceStation = 60,     // Comisaría
    FireStation = 61,       // Bomberos
    EmergencyCenter = 62,   // Centro emergencias 112
    Courthouse = 63,        // Juzgados
    Prison = 64,            // Cárcel
    
    // Administración pública
    CityHall = 70,          // Ayuntamiento
    GovernmentOffice = 71,  // Oficinas gobierno
    PostOffice = 72,        // Correos
    DMV = 73,               // Tráfico / DGT
    SocialServices = 74,    // Servicios sociales
    
    // Cultura / Ocio
    Museum = 80,            // Museo
    Theater = 81,           // Teatro
    Cinema = 82,            // Cine
    ConcertHall = 83,       // Auditorio
    Stadium = 84,           // Estadio
    SportsCenter = 85,      // Polideportivo
    Park = 86,              // Parque
    Plaza = 87,             // Plaza pública
    CommunityCenter = 88,   // Centro cívico
    
    // Hostelería / Turismo
    Hotel = 90,             // Hotel
    Hostel = 91,            // Hostal
    Restaurant = 92,        // Restaurante
    Cafe = 93,              // Cafetería
    Bar = 94,               // Bar / pub
    Nightclub = 95,         // Discoteca
    
    // Transporte
    TrainStation = 100,     // Estación tren
    MetroStation = 101,     // Estación metro
    BusTerminal = 102,      // Terminal autobuses
    Airport = 103,          // Aeropuerto
    ParkingGarage = 104,    // Parking público
    TaxiStand = 105,        // Parada taxis
    BikeShareStation = 106, // Bicing / bici pública
    
    // Servicios
    Bank = 110,             // Banco
    GasStation = 111,       // Gasolinera
    CarRepair = 112,        // Taller coches
    Laundromat = 113,       // Lavandería
    Gym = 114,              // Gimnasio
    HairSalon = 115,        // Peluquería
    
    // Religioso
    Church = 120,           // Iglesia
    Mosque = 121,           // Mezquita
    Synagogue = 122,        // Sinagoga
    Temple = 123,           // Templo
    
    // Especial
    Landmark = 200,         // Punto de referencia turístico
    ConstructionSite = 201, // Obra en construcción
    VacantLot = 202         // Solar vacío
};

/// Horario de apertura
struct OpeningHours {
    struct DaySchedule {
        bool closed = false;
        float open_hour = 9.f;   // 9:00
        float close_hour = 18.f; // 18:00
        std::vector<std::pair<float, float>> breaks; // Pausas (ej: comida 14-15)
    };
    std::array<DaySchedule, 7> days; // 0=Lunes ... 6=Domingo
    std::vector<std::pair<int, DaySchedule>> exceptions; // Festivos: (day_of_year, schedule)
    
    bool is_open(float world_hour, int week_day, int day_of_year) const;
    float time_until_open(float world_hour, int week_day, int day_of_year) const;
    float time_until_close(float world_hour, int week_day, int day_of_year) const;
};

/// Zona interior de un edificio (lobby, oficina, sala, etc.)
struct InteriorZone {
    std::uint32_t id;
    std::string name;
    Vec2 position;           // Posición relativa al edificio
    float radius = 5.f;      // Radio de la zona
    std::uint32_t capacity = 50; // Capacidad máxima
    std::uint32_t current_occupancy = 0;
    std::vector<std::uint32_t> agent_ids; // Agentes dentro
    BuildingType zone_type = BuildingType::OfficeSmall;
    bool is_restricted = false; // Requiere autorización
    std::vector<std::uint32_t> connected_zones; // Zonas adyacentes
    std::vector<std::uint32_t> entrance_nodes; // Nodos de entrada desde grafo
    
    bool can_enter() const { return current_occupancy < capacity; }
    void enter(std::uint32_t agent_id) { 
        if (can_enter()) { agent_ids.push_back(agent_id); current_occupancy++; }
    }
    void exit(std::uint32_t agent_id) {
        auto it = std::find(agent_ids.begin(), agent_ids.end(), agent_id);
        if (it != agent_ids.end()) { agent_ids.erase(it); current_occupancy--; }
    }
    float occupancy_ratio() const { return capacity > 0 ? float(current_occupancy) / capacity : 0.f; }
};

/// Edificio completo
struct Building {
    std::uint32_t id;
    BuildingType type = BuildingType::House;
    std::string name;
    Vec2 position;           // Centro del edificio
    Vec2 size;               // Ancho, alto (bounding box)
    float rotation = 0.f;    // Rotación en radianes
    
    // Capacidad y ocupación
    std::uint32_t max_capacity = 100;
    std::uint32_t current_occupancy = 0;
    std::vector<std::uint32_t> occupant_ids; // Todos los agentes dentro
    
    // Zonas interiores
    std::vector<InteriorZone> zones;
    std::uint32_t main_entrance_zone = 0;
    
    // Puntos de entrada/salida (conectan con grafo de navegación)
    std::vector<Vec2> entrances;        // Posiciones absolutas
    std::vector<std::uint32_t> entrance_node_ids; // Nodos del NavGraph
    std::vector<std::uint32_t> exit_node_ids;     // Nodos de salida (puede ser distinto)
    
    // Horarios
    OpeningHours hours;
    bool is_24h = false;
    
    // Empleos asociados
    struct JobSlot {
        std::uint32_t id;
        std::string title;
        BuildingType department; // Tipo de zona donde se trabaja
        float salary_multiplier = 1.f;
        std::uint8_t required_education = 0; // 0=none, 1=secondary, 2=university, 3=phd
        std::uint32_t max_workers = 1;
        std::vector<std::uint32_t> worker_ids;
        std::uint32_t zone_id = 0; // Zona interior donde se trabaja
    };
    std::vector<JobSlot> jobs;
    
    // Residencial (si es vivienda)
    struct Household {
        std::uint32_t id;
        std::uint32_t zone_id; // Zona interior (piso, casa)
        std::vector<std::uint32_t> resident_ids;
        float rent = 0.f;
        bool owns = false;
    };
    std::vector<Household> households;
    
    // Estado
    bool is_operational = true;
    float condition = 1.f; // 0-1, estado del edificio
    std::uint32_t build_year = 2000;
    std::uint32_t last_renovation = 2020;
    
    // Métodos
    bool can_accommodate(std::uint32_t count = 1) const {
        return current_occupancy + count <= max_capacity;
    }
    
    bool enter_building(std::uint32_t agent_id, std::uint32_t zone_id = 0) {
        if (!can_accommodate()) return false;
        if (zone_id < zones.size() && !zones[zone_id].can_enter()) return false;
        
        occupant_ids.push_back(agent_id);
        current_occupancy++;
        if (zone_id < zones.size()) zones[zone_id].enter(agent_id);
        return true;
    }
    
    void exit_building(std::uint32_t agent_id, std::uint32_t zone_id = 0) {
        auto it = std::find(occupant_ids.begin(), occupant_ids.end(), agent_id);
        if (it != occupant_ids.end()) {
            occupant_ids.erase(it);
            current_occupancy--;
        }
        if (zone_id < zones.size()) zones[zone_id].exit(agent_id);
    }
    
    std::optional<std::uint32_t> find_available_job(std::uint8_t education_level) const;
    std::optional<std::uint32_t> find_available_housing(std::uint32_t household_size) const;
    Vec2 get_random_entrance() const;
    Vec2 get_entrance_for_zone(std::uint32_t zone_id) const;
    bool is_open(float world_hour, int week_day, int day_of_year) const;
};

/// Configuración por tipo de edificio (data-driven)
struct BuildingTypeConfig {
    BuildingType type;
    std::string name;
    Vec2 typical_size;        // Tamaño típico (ancho, alto)
    std::uint32_t base_capacity;
    std::uint32_t max_floors = 1;
    float footprint_ratio = 0.6f; // Ratio huella/solar
    
    // Zonas interiores típicas
    struct ZoneTemplate {
        std::string name;
        BuildingType zone_type;
        float size_ratio;      // Fracción del edificio
        std::uint32_t capacity_per_sqm = 1;
        bool is_public = true;
        std::vector<std::string> connected_to; // Nombres de zonas conectadas
    };
    std::vector<ZoneTemplate> zone_templates;
    
    // Jobs típicos
    struct JobTemplate {
        std::string title;
        BuildingType department;
        float salary_multiplier = 1.f;
        std::uint8_t required_education = 0;
        float workers_per_sqm = 0.01f; // Densidad de trabajadores
    };
    std::vector<JobTemplate> job_templates;
    
    // Horarios por defecto
    OpeningHours default_hours;
    bool is_24h = false;
    
    // Residencial
    float residential_ratio = 0.f; // Fracción para vivienda
    std::uint32_t units_per_floor = 4;
    float avg_unit_size = 80.f; // m2
    
    // Costes
    float construction_cost_per_sqm = 1500.f;
    float monthly_maintenance_per_sqm = 5.f;
    
    // Atracción (para pathfinding / decisiones)
    float visit_attraction = 1.f;    // Qué tan atractivo es visitar
    float work_attraction = 1.f;     // Qué tan atractivo para trabajar
    float live_attraction = 1.f;     // Qué tan atractivo para vivir
    
    static const std::unordered_map<BuildingType, BuildingTypeConfig>& all_configs();
    static const BuildingTypeConfig& get(BuildingType type);
};

/// Registro de todos los edificios del mundo
struct BuildingRegistry {
    std::unordered_map<std::uint32_t, Building> buildings;
    std::uint32_t next_id = 1;
    
    std::uint32_t add(Building&& building) {
        building.id = next_id++;
        auto id = building.id;
        buildings.emplace(id, std::move(building));
        return id;
    }
    
    Building* get(std::uint32_t id) {
        auto it = buildings.find(id);
        return it != buildings.end() ? &it->second : nullptr;
    }
    
    const Building* get(std::uint32_t id) const {
        auto it = buildings.find(id);
        return it != buildings.end() ? &it->second : nullptr;
    }
    
    void remove(std::uint32_t id) { buildings.erase(id); }
    
    std::vector<std::uint32_t> find_by_type(BuildingType type) const;
    std::vector<std::uint32_t> find_near(Vec2 pos, float radius) const;
    std::vector<std::uint32_t> find_with_capacity(std::uint32_t min_capacity) const;
    std::vector<std::uint32_t> find_hiring(std::uint8_t education_level) const;
    std::vector<std::uint32_t> find_with_housing(std::uint32_t household_size) const;
    
    // Estadísticas
    std::uint32_t total_capacity() const;
    std::uint32_t total_occupancy() const;
    float average_occupancy_ratio() const;
    std::unordered_map<BuildingType, std::uint32_t> count_by_type() const;
};

} // namespace npc_crowd::world
