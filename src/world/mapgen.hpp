#pragma once
// Procedural city generator driven by data/maps/templates.json.

#include "world/city_map.hpp"

#include <string>
#include <vector>

namespace npc::world {

struct ZoningRule {
    enum class Shape : std::uint8_t { Circle, Rect, BandRiver, BandCoast, Default } shape = Shape::Default;
    int district = -1;                               // DistrictKind (not for Default)
    float a = 0, b = 0, c = 0, d = 0;                // circle: cx, cy, r | rect: x0, y0, x1, y1 | band: width
    float chance = 1.f;
    std::vector<std::pair<int, float>> weights;      // Default rule
};

struct MapTemplate {
    std::string id;
    float spacing[2] = {80, 70};
    float jitter = 0, warp = 0, remove = 0, path_ratio = 0.5f;
    int avenue_every = 0;
    bool diagonal = false;
    std::string water = "none";                      // none | river | coast
    float water_width = 60, water_amplitude = 50, water_level = 0.78f;
    int bridge_every = 3;
    std::vector<ZoningRule> zoning;
};

struct MapSize {
    std::string id;
    float width = 1000, height = 800;
    int population = 3000;
};

struct NamePools {
    std::vector<std::string> streets, prefixes, districts;
};

class MapLibrary {
public:
    /// Loads data/maps/templates.json and names.json.
    bool load(const std::string& data_dir, const data::Catalog& catalog, std::string& error);
    const MapTemplate* find_template(std::string_view id) const;
    const MapSize* find_size(std::string_view id) const;
    std::vector<MapTemplate> templates;
    std::vector<MapSize> sizes;
    NamePools names;
};

struct MapSpec {
    std::string template_id = "grid";
    std::string size_id = "small";
    std::uint64_t seed = 42;
    std::string name;
};

/// Generates and finalizes a city. Returns false with an error message if the spec is invalid.
bool generate_map(const MapSpec& spec, const MapLibrary& lib, const data::Catalog& catalog, CityMap& out, std::string& error);

} // namespace npc::world
