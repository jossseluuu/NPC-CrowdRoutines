#pragma once
// City map: road graph, neighbourhoods, blocks, buildings, water and bus lines.
// Produced by the template generators (mapgen.hpp) or loaded from a custom JSON map (format "npc-city-map").

#include "core/json.hpp"
#include "core/math.hpp"
#include "data/catalog.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace npc::world {

enum class RoadKind : std::uint8_t { Street, Avenue, Path, Bridge, Promenade };
const char* road_kind_name(RoadKind k);
bool parse_road_kind(std::string_view s, RoadKind& out);

struct Node {
    Vec2 pos;
    std::vector<std::uint32_t> edges;
    bool traffic_light = false;
};

struct Edge {
    std::uint32_t a = 0, b = 0;
    RoadKind kind = RoadKind::Street;
    float length = 0;
    int street = -1;          // index into street_names
    bool cars() const { return kind != RoadKind::Path; }
    float car_speed() const;  // m/s speed limit
    std::uint32_t other(std::uint32_t n) const { return n == a ? b : a; }
};

struct District {             // named neighbourhood
    std::string name;
    int kind = 0;             // dominant DistrictKind index
    Vec2 center;
};

struct Block {
    Polygon polygon;
    int district = 0;         // neighbourhood index
    int kind = 0;             // DistrictKind index (land use of this block)
};

struct Building {
    std::uint32_t id = 0;
    int type = 0;             // BuildingType index
    int category = 0;         // cached BuildingType::category
    Polygon footprint;
    Vec2 center;
    int district = 0;
    int floors = 1;
    int households = 0, jobs = 0, students = 0, visitors = 0;
    std::uint32_t entrance_node = 0;
    std::string name;
};

struct WaterBody {
    std::string kind;         // river | sea | lake
    Polygon polygon;
};

struct BusLine {
    std::string name, color;
    std::vector<std::uint32_t> path;   // node ids, travelled back and forth
    std::vector<std::uint32_t> stops;  // node ids (subset of path)
};

class CityMap {
public:
    std::string name, template_id = "custom", size_id = "custom";
    std::uint64_t seed = 0;
    float width = 1000, height = 1000;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<std::string> street_names;
    std::vector<District> districts;
    std::vector<Block> blocks;
    std::vector<Building> buildings;
    std::vector<WaterBody> water;
    std::vector<BusLine> bus_lines;
    int suggested_population = 0;

    // Derived data (rebuilt by finalize)
    std::vector<std::vector<std::uint32_t>> buildings_by_category;
    std::vector<std::vector<std::uint32_t>> buildings_by_type;

    /// Computes lengths, removes disconnected parts, assigns entrances, traffic lights, capacities and indexes.
    /// Returns false (with a message) if the map is unusable.
    bool finalize(const data::Catalog& catalog, std::string& error, bool auto_lights = true, bool auto_bus = true);

    std::uint32_t nearest_node(Vec2 p) const;
    std::uint32_t nearest_node_with_cars(Vec2 p) const;
    /// Edge index connecting two adjacent nodes, or UINT32_MAX.
    std::uint32_t edge_between(std::uint32_t a, std::uint32_t b) const;
    int district_at(Vec2 p) const;
    std::size_t total_households() const;
    std::size_t total_jobs() const;

    json::Value to_json(const data::Catalog& catalog) const;
    /// Loads a map in format "npc-city-map" (custom or exported). Call finalize() afterwards.
    static bool from_json(const json::Value& v, const data::Catalog& catalog, CityMap& out, std::string& error);

private:
    void build_spatial_index();
    std::vector<std::vector<std::uint32_t>> grid_;
    float cell_ = 60.f;
    int gw_ = 1, gh_ = 1;
};

/// Shortest bus routes / helpers shared by the generator and finalize().
std::vector<std::uint32_t> shortest_node_path(const CityMap& map, std::uint32_t from, std::uint32_t to, bool cars_only,
                                              bool prefer_avenues);

} // namespace npc::world
