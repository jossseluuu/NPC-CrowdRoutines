#include "world/city_map.hpp"

#include "core/rng.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_set>

namespace npc::world {

namespace {
constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();
const char* kBusColors[] = {"#ff6b6b", "#4dabf7", "#ffd43b", "#69db7c", "#da77f2", "#ff922b", "#3bc9db", "#f783ac"};

json::Value poly_json(const Polygon& p) {
    json::Value arr = json::Value::array();
    for (auto v : p) {
        json::Value pt = json::Value::array();
        pt.push_back(std::round(v.x * 10) / 10);
        pt.push_back(std::round(v.y * 10) / 10);
        arr.push_back(std::move(pt));
    }
    return arr;
}

bool parse_poly(const json::Value& v, Polygon& out) {
    out.clear();
    for (const auto& p : v.as_array()) {
        if (!p.is_array() || p.size() < 2) return false;
        out.push_back({p[0].as_float(), p[1].as_float()});
    }
    return out.size() >= 3;
}
} // namespace

const char* road_kind_name(RoadKind k) {
    switch (k) {
        case RoadKind::Street: return "street";
        case RoadKind::Avenue: return "avenue";
        case RoadKind::Path: return "path";
        case RoadKind::Bridge: return "bridge";
        case RoadKind::Promenade: return "promenade";
    }
    return "street";
}

bool parse_road_kind(std::string_view s, RoadKind& out) {
    if (s == "street") out = RoadKind::Street;
    else if (s == "avenue") out = RoadKind::Avenue;
    else if (s == "path") out = RoadKind::Path;
    else if (s == "bridge") out = RoadKind::Bridge;
    else if (s == "promenade") out = RoadKind::Promenade;
    else return false;
    return true;
}

float Edge::car_speed() const {
    switch (kind) {
        case RoadKind::Avenue: return 13.9f;   // 50 km/h
        case RoadKind::Bridge: return 11.1f;   // 40 km/h
        case RoadKind::Promenade: return 5.6f; // 20 km/h
        case RoadKind::Path: return 0.f;
        case RoadKind::Street: break;
    }
    return 8.3f;                               // 30 km/h
}

// ---------------------------------------------------------------------------------------------
std::vector<std::uint32_t> shortest_node_path(const CityMap& map, std::uint32_t from, std::uint32_t to, bool cars_only,
                                              bool prefer_avenues) {
    const std::size_t n = map.nodes.size();
    if (from >= n || to >= n) return {};
    std::vector<float> dist(n, std::numeric_limits<float>::infinity());
    std::vector<std::uint32_t> prev(n, kNone);
    using Item = std::pair<float, std::uint32_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    dist[from] = 0;
    open.emplace(0.f, from);
    while (!open.empty()) {
        const auto [d, u] = open.top();
        open.pop();
        if (u == to) break;
        if (d > dist[u]) continue;
        for (auto ei : map.nodes[u].edges) {
            const auto& e = map.edges[ei];
            if (cars_only && !e.cars()) continue;
            const float w = e.length * (prefer_avenues && (e.kind == RoadKind::Avenue || e.kind == RoadKind::Bridge) ? 0.55f : 1.f);
            const auto v = e.other(u);
            if (d + w < dist[v]) {
                dist[v] = d + w;
                prev[v] = u;
                open.emplace(dist[v], v);
            }
        }
    }
    if (from != to && prev[to] == kNone) return {};
    std::vector<std::uint32_t> path;
    for (auto v = to; v != kNone; v = prev[v]) {
        path.push_back(v);
        if (v == from) break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ---------------------------------------------------------------------------------------------
void CityMap::build_spatial_index() {
    cell_ = 60.f;
    gw_ = std::max(1, static_cast<int>(width / cell_) + 1);
    gh_ = std::max(1, static_cast<int>(height / cell_) + 1);
    grid_.assign(static_cast<std::size_t>(gw_ * gh_), {});
    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        const int cx = std::clamp(static_cast<int>(nodes[i].pos.x / cell_), 0, gw_ - 1);
        const int cy = std::clamp(static_cast<int>(nodes[i].pos.y / cell_), 0, gh_ - 1);
        grid_[static_cast<std::size_t>(cy * gw_ + cx)].push_back(i);
    }
}

std::uint32_t CityMap::nearest_node(Vec2 p) const {
    if (nodes.empty()) return 0;
    const int cx = std::clamp(static_cast<int>(p.x / cell_), 0, gw_ - 1);
    const int cy = std::clamp(static_cast<int>(p.y / cell_), 0, gh_ - 1);
    std::uint32_t best = kNone;
    float best_d = std::numeric_limits<float>::infinity();
    for (int r = 0; r < std::max(gw_, gh_); ++r) {
        for (int y = cy - r; y <= cy + r; ++y)
            for (int x = cx - r; x <= cx + r; ++x) {
                if (x < 0 || y < 0 || x >= gw_ || y >= gh_) continue;
                if (std::max(std::abs(x - cx), std::abs(y - cy)) != r) continue; // ring only
                for (auto i : grid_[static_cast<std::size_t>(y * gw_ + x)]) {
                    const float d = length_sq(nodes[i].pos - p);
                    if (d < best_d) { best_d = d; best = i; }
                }
            }
        // Once a candidate exists, one more ring guarantees correctness.
        if (best != kNone && static_cast<float>(r) * cell_ > std::sqrt(best_d) + cell_) break;
    }
    return best == kNone ? 0 : best;
}

std::uint32_t CityMap::nearest_node_with_cars(Vec2 p) const {
    std::uint32_t best = nearest_node(p);
    bool ok = false;
    for (auto ei : nodes[best].edges) ok |= edges[ei].cars();
    if (ok) return best;
    float best_d = std::numeric_limits<float>::infinity();
    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        bool cars = false;
        for (auto ei : nodes[i].edges) cars |= edges[ei].cars();
        if (!cars) continue;
        const float d = length_sq(nodes[i].pos - p);
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

std::uint32_t CityMap::edge_between(std::uint32_t a, std::uint32_t b) const {
    for (auto ei : nodes[a].edges)
        if (edges[ei].other(a) == b) return ei;
    return kNone;
}

int CityMap::district_at(Vec2 p) const {
    int best = 0;
    float best_d = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < districts.size(); ++i) {
        const float d = length_sq(districts[i].center - p);
        if (d < best_d) { best_d = d; best = static_cast<int>(i); }
    }
    return best;
}

std::size_t CityMap::total_households() const {
    std::size_t n = 0;
    for (const auto& b : buildings) n += static_cast<std::size_t>(b.households);
    return n;
}
std::size_t CityMap::total_jobs() const {
    std::size_t n = 0;
    for (const auto& b : buildings) n += static_cast<std::size_t>(b.jobs);
    return n;
}

// ---------------------------------------------------------------------------------------------
bool CityMap::finalize(const data::Catalog& cat, std::string& error, bool auto_lights, bool auto_bus) {
    if (nodes.size() < 4 || edges.size() < 3) { error = "map needs at least 4 nodes and 3 edges"; return false; }

    // 1. Validate edges, drop duplicates / degenerate ones, rebuild adjacency.
    {
        std::unordered_set<std::uint64_t> seen;
        std::vector<Edge> clean;
        for (const auto& e : edges) {
            if (e.a >= nodes.size() || e.b >= nodes.size()) { error = "edge references a missing node"; return false; }
            if (e.a == e.b) continue;
            const std::uint64_t key = (static_cast<std::uint64_t>(std::min(e.a, e.b)) << 32) | std::max(e.a, e.b);
            if (!seen.insert(key).second) continue;
            clean.push_back(e);
        }
        edges = std::move(clean);
    }

    // 2. Keep the largest connected component.
    std::vector<int> comp(nodes.size(), -1);
    {
        std::vector<std::vector<std::uint32_t>> adj(nodes.size());
        for (const auto& e : edges) { adj[e.a].push_back(e.b); adj[e.b].push_back(e.a); }
        int best = -1;
        std::size_t best_size = 0;
        int c = 0;
        for (std::uint32_t s = 0; s < nodes.size(); ++s) {
            if (comp[s] >= 0 || adj[s].empty()) continue;
            std::size_t size = 0;
            std::vector<std::uint32_t> stack{s};
            comp[s] = c;
            while (!stack.empty()) {
                const auto u = stack.back();
                stack.pop_back();
                ++size;
                for (auto v : adj[u])
                    if (comp[v] < 0) { comp[v] = c; stack.push_back(v); }
            }
            if (size > best_size) { best_size = size; best = c; }
            ++c;
        }
        std::vector<std::uint32_t> remap(nodes.size(), kNone);
        std::vector<Node> kept;
        for (std::uint32_t i = 0; i < nodes.size(); ++i) {
            if (comp[i] != best) continue;
            remap[i] = static_cast<std::uint32_t>(kept.size());
            kept.push_back(nodes[i]);
        }
        std::vector<Edge> kept_edges;
        for (auto e : edges) {
            if (remap[e.a] == kNone || remap[e.b] == kNone) continue;
            e.a = remap[e.a];
            e.b = remap[e.b];
            kept_edges.push_back(e);
        }
        for (auto& line : bus_lines) {
            auto fix = [&](std::vector<std::uint32_t>& v) {
                std::vector<std::uint32_t> out;
                for (auto n : v)
                    if (n < remap.size() && remap[n] != kNone) out.push_back(remap[n]);
                v = std::move(out);
            };
            fix(line.path);
            fix(line.stops);
        }
        nodes = std::move(kept);
        edges = std::move(kept_edges);
    }
    if (nodes.size() < 4) { error = "road network is too small after removing disconnected parts"; return false; }
    for (auto& n : nodes) n.edges.clear();
    for (std::uint32_t i = 0; i < edges.size(); ++i) {
        auto& e = edges[i];
        e.length = std::max(0.5f, distance(nodes[e.a].pos, nodes[e.b].pos));
        if (e.street >= static_cast<int>(street_names.size())) e.street = -1;
        nodes[e.a].edges.push_back(i);
        nodes[e.b].edges.push_back(i);
    }
    bool any_car = false;
    for (const auto& e : edges) any_car |= e.cars();
    if (!any_car) { error = "map has no roads usable by cars"; return false; }

    // Extent from geometry if needed.
    for (const auto& n : nodes) { width = std::max(width, n.pos.x + 10); height = std::max(height, n.pos.y + 10); }
    build_spatial_index();

    // 3. Neighbourhoods.
    if (districts.empty()) {
        District d;
        d.name = name.empty() ? "Centro" : name;
        d.kind = std::max(0, cat.district_index("residential_mid"));
        d.center = {width / 2, height / 2};
        districts.push_back(d);
    }
    for (auto& d : districts) d.kind = std::clamp(d.kind, 0, static_cast<int>(cat.districts.size()) - 1);

    // 4. Buildings: geometry, capacities, entrances.
    Rng rng(seed, "finalize-buildings");
    for (std::uint32_t i = 0; i < buildings.size(); ++i) {
        auto& b = buildings[i];
        b.id = i;
        b.type = std::clamp(b.type, 0, static_cast<int>(cat.building_types.size()) - 1);
        const auto& bt = cat.building_types[static_cast<std::size_t>(b.type)];
        b.category = bt.category;
        b.center = polygon_centroid(b.footprint);
        if (b.district < 0 || b.district >= static_cast<int>(districts.size())) b.district = district_at(b.center);
        if (b.floors <= 0) b.floors = std::max(bt.floors[0], (bt.floors[0] + bt.floors[1]) / 2);
        const float area_factor = std::clamp(polygon_area(b.footprint) / std::max(1.f, (bt.size[0] + bt.size[1]) * (bt.size[2] + bt.size[3]) / 4.f), 0.4f, 3.f);
        auto pick = [&](const int range[2]) {
            if (range[1] <= 0) return 0;
            const float span = static_cast<float>(range[1] - range[0]);
            return std::max(range[0] > 0 ? 1 : 0, static_cast<int>(std::round((range[0] + span * rng.uniformf()) * std::sqrt(area_factor))));
        };
        if (b.households <= 0) b.households = pick(bt.households);
        if (b.jobs <= 0) b.jobs = pick(bt.jobs);
        if (b.students <= 0) b.students = pick(bt.students);
        if (b.visitors <= 0) b.visitors = std::max(bt.visitors, 0);
        b.entrance_node = nearest_node(b.center);
    }
    if (buildings.empty()) { error = "map has no buildings"; return false; }

    // 5. Traffic lights on busy intersections.
    bool any_light = false;
    for (const auto& n : nodes) any_light |= n.traffic_light;
    if (auto_lights && !any_light) {
        for (std::uint32_t i = 0; i < nodes.size(); ++i) {
            auto& n = nodes[i];
            int car_edges = 0, major = 0;
            for (auto ei : n.edges) {
                if (!edges[ei].cars()) continue;
                ++car_edges;
                major += edges[ei].kind == RoadKind::Avenue || edges[ei].kind == RoadKind::Bridge;
            }
            n.traffic_light = car_edges >= 3 && (major >= 1 || (car_edges >= 4 && (hash_combine(seed, i) % 100) < 25));
        }
    }

    // 6. Bus network.
    if (auto_bus && bus_lines.empty()) {
        Rng brng(seed, "bus-lines");
        const int count = std::clamp(static_cast<int>(width * height / (520.f * 520.f)), 2, 8);
        for (int l = 0; l < count; ++l) {
            Vec2 a, b;
            const float t1 = brng.range(0.1f, 0.9f), t2 = brng.range(0.1f, 0.9f);
            switch (l % 3) {
                case 0: a = {width * 0.04f, height * t1}; b = {width * 0.96f, height * t2}; break;
                case 1: a = {width * t1, height * 0.04f}; b = {width * t2, height * 0.96f}; break;
                default: a = {width * 0.06f, height * 0.06f + height * 0.3f * t1}; b = {width * 0.94f, height * 0.94f - height * 0.3f * t2}; break;
            }
            BusLine line;
            line.name = "L" + std::to_string(l + 1);
            line.color = kBusColors[l % 8];
            line.path = shortest_node_path(*this, nearest_node_with_cars(a), nearest_node_with_cars(b), true, true);
            if (line.path.size() < 3) continue;
            float acc = 1e9f;
            for (std::size_t k = 0; k < line.path.size(); ++k) {
                if (k > 0) acc += distance(nodes[line.path[k - 1]].pos, nodes[line.path[k]].pos);
                if (acc >= 260.f || k + 1 == line.path.size()) { line.stops.push_back(line.path[k]); acc = 0; }
            }
            bus_lines.push_back(std::move(line));
        }
    }
    for (auto& line : bus_lines)
        if (line.stops.empty() && !line.path.empty()) line.stops = {line.path.front(), line.path.back()};

    // 7. Indexes.
    buildings_by_category.assign(cat.categories.size(), {});
    buildings_by_type.assign(cat.building_types.size(), {});
    for (const auto& b : buildings) {
        buildings_by_category[static_cast<std::size_t>(b.category)].push_back(b.id);
        buildings_by_type[static_cast<std::size_t>(b.type)].push_back(b.id);
    }
    if (total_households() == 0) { error = "map has no residential buildings (no homes for residents)"; return false; }
    return true;
}

// ---------------------------------------------------------------------------------------------
json::Value CityMap::to_json(const data::Catalog& cat) const {
    json::Value v = json::Value::object();
    v.set("format", "npc-city-map");
    v.set("version", 1);
    v.set("name", name);
    v.set("template", template_id);
    v.set("size", size_id);
    v.set("seed", static_cast<double>(seed));
    v.set("width", width);
    v.set("height", height);
    v.set("suggested_population", suggested_population);
    json::Value ns = json::Value::array(), lights = json::Value::array();
    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        json::Value p = json::Value::array();
        p.push_back(std::round(nodes[i].pos.x * 10) / 10);
        p.push_back(std::round(nodes[i].pos.y * 10) / 10);
        ns.push_back(std::move(p));
        if (nodes[i].traffic_light) lights.push_back(i);
    }
    v.set("nodes", std::move(ns));
    v.set("lights", std::move(lights));
    json::Value es = json::Value::array();
    for (const auto& e : edges) {
        json::Value j = json::Value::array();
        j.push_back(e.a);
        j.push_back(e.b);
        j.push_back(road_kind_name(e.kind));
        j.push_back(e.street);
        es.push_back(std::move(j));
    }
    v.set("edges", std::move(es));
    json::Value sn = json::Value::array();
    for (const auto& s : street_names) sn.push_back(s);
    v.set("street_names", std::move(sn));
    json::Value ds = json::Value::array();
    for (const auto& d : districts) {
        json::Value j = json::Value::object();
        j.set("name", d.name);
        j.set("kind", cat.districts[static_cast<std::size_t>(d.kind)].id);
        json::Value c = json::Value::array();
        c.push_back(std::round(d.center.x));
        c.push_back(std::round(d.center.y));
        j.set("center", std::move(c));
        ds.push_back(std::move(j));
    }
    v.set("districts", std::move(ds));
    json::Value bl = json::Value::array();
    for (const auto& b : blocks) {
        json::Value j = json::Value::object();
        j.set("district", b.district);
        j.set("kind", cat.districts[static_cast<std::size_t>(b.kind)].id);
        j.set("polygon", poly_json(b.polygon));
        bl.push_back(std::move(j));
    }
    v.set("blocks", std::move(bl));
    json::Value bs = json::Value::array();
    for (const auto& b : buildings) {
        json::Value j = json::Value::object();
        j.set("type", cat.building_types[static_cast<std::size_t>(b.type)].id);
        j.set("district", b.district);
        j.set("floors", b.floors);
        j.set("households", b.households);
        j.set("jobs", b.jobs);
        if (b.students) j.set("students", b.students);
        if (!b.name.empty()) j.set("name", b.name);
        j.set("polygon", poly_json(b.footprint));
        bs.push_back(std::move(j));
    }
    v.set("buildings", std::move(bs));
    json::Value ws = json::Value::array();
    for (const auto& w : water) {
        json::Value j = json::Value::object();
        j.set("kind", w.kind);
        j.set("polygon", poly_json(w.polygon));
        ws.push_back(std::move(j));
    }
    v.set("water", std::move(ws));
    json::Value ls = json::Value::array();
    for (const auto& l : bus_lines) {
        json::Value j = json::Value::object();
        j.set("name", l.name);
        j.set("color", l.color);
        json::Value p = json::Value::array(), s = json::Value::array();
        for (auto n : l.path) p.push_back(n);
        for (auto n : l.stops) s.push_back(n);
        j.set("path", std::move(p));
        j.set("stops", std::move(s));
        ls.push_back(std::move(j));
    }
    v.set("bus_lines", std::move(ls));
    return v;
}

bool CityMap::from_json(const json::Value& v, const data::Catalog& cat, CityMap& m, std::string& err) {
    m = CityMap{};
    if (v.str("format") != "npc-city-map") { err = "not a map file (format must be \"npc-city-map\")"; return false; }
    if (v.integer("version", 1) > 1) { err = "map format version " + std::to_string(v.integer("version", 1)) + " is newer than supported (1)"; return false; }
    m.name = v.str("name", "Custom map");
    m.template_id = v.str("template", "custom");
    m.size_id = v.str("size", "custom");
    m.seed = static_cast<std::uint64_t>(v.num("seed", 1));
    m.width = v.flt("width", 0);
    m.height = v.flt("height", 0);
    m.suggested_population = static_cast<int>(v.integer("suggested_population", 0));
    for (const auto& p : v["nodes"].as_array()) {
        if (!p.is_array() || p.size() < 2) { err = "nodes must be [x, y] pairs"; return false; }
        Node n;
        n.pos = {p[0].as_float(), p[1].as_float()};
        if (n.pos.x < 0 || n.pos.y < 0) { err = "node coordinates must be >= 0"; return false; }
        m.nodes.push_back(n);
    }
    for (const auto& l : v["lights"].as_array()) {
        const auto i = static_cast<std::size_t>(l.as_int(-1));
        if (i < m.nodes.size()) m.nodes[i].traffic_light = true;
    }
    for (const auto& s : v["street_names"].as_array()) m.street_names.push_back(s.as_string());
    std::size_t idx = 0;
    for (const auto& e : v["edges"].as_array()) {
        Edge ed;
        if (!e.is_array() || e.size() < 2) { err = "edge #" + std::to_string(idx) + " must be [a, b, kind?, street?]"; return false; }
        const auto a = e[0].as_int(-1), b = e[1].as_int(-1);
        if (a < 0 || b < 0 || static_cast<std::size_t>(a) >= m.nodes.size() || static_cast<std::size_t>(b) >= m.nodes.size()) {
            err = "edge #" + std::to_string(idx) + " references a missing node";
            return false;
        }
        ed.a = static_cast<std::uint32_t>(a);
        ed.b = static_cast<std::uint32_t>(b);
        if (e.size() > 2 && !parse_road_kind(e[2].as_string("street"), ed.kind)) {
            err = "edge #" + std::to_string(idx) + ": unknown road kind '" + e[2].as_string("") + "'";
            return false;
        }
        ed.street = static_cast<int>(e[3].as_int(-1));
        m.edges.push_back(ed);
        ++idx;
    }
    for (const auto& d : v["districts"].as_array()) {
        District dist;
        dist.name = d.str("name", "Barrio");
        dist.kind = cat.district_index(d.str("kind", "residential_mid"));
        if (dist.kind < 0) { err = "district '" + dist.name + "': unknown kind '" + d.str("kind") + "'"; return false; }
        dist.center = {d["center"][0].as_float(-1), d["center"][1].as_float(-1)};
        m.districts.push_back(dist);
    }
    for (const auto& b : v["blocks"].as_array()) {
        Block bl;
        if (!parse_poly(b["polygon"], bl.polygon)) continue;
        bl.district = static_cast<int>(b.integer("district", 0));
        bl.kind = std::max(0, cat.district_index(b.str("kind", "residential_mid")));
        m.blocks.push_back(std::move(bl));
    }
    idx = 0;
    for (const auto& b : v["buildings"].as_array()) {
        Building bd;
        bd.type = cat.building_index(b.str("type"));
        if (bd.type < 0) { err = "building #" + std::to_string(idx) + ": unknown type '" + b.str("type") + "'"; return false; }
        if (b.contains("polygon")) {
            if (!parse_poly(b["polygon"], bd.footprint)) { err = "building #" + std::to_string(idx) + ": polygon needs 3+ points"; return false; }
        } else if (b.contains("center")) {
            // Rectangle shorthand: "center": [x, y], "size": [w, h]
            const Vec2 c{b["center"][0].as_float(), b["center"][1].as_float()};
            const auto& bt = cat.building_types[static_cast<std::size_t>(bd.type)];
            const float w = b["size"][0].as_float((bt.size[0] + bt.size[1]) / 2), h = b["size"][1].as_float((bt.size[2] + bt.size[3]) / 2);
            bd.footprint = {{c.x - w / 2, c.y - h / 2}, {c.x + w / 2, c.y - h / 2}, {c.x + w / 2, c.y + h / 2}, {c.x - w / 2, c.y + h / 2}};
        } else {
            err = "building #" + std::to_string(idx) + ": needs \"polygon\" or \"center\"";
            return false;
        }
        bd.district = static_cast<int>(b.integer("district", -1));
        bd.floors = static_cast<int>(b.integer("floors", 0));
        bd.households = static_cast<int>(b.integer("households", 0));
        bd.jobs = static_cast<int>(b.integer("jobs", 0));
        bd.students = static_cast<int>(b.integer("students", 0));
        bd.name = b.str("name");
        m.buildings.push_back(std::move(bd));
        ++idx;
    }
    for (const auto& w : v["water"].as_array()) {
        WaterBody wb;
        wb.kind = w.str("kind", "lake");
        if (parse_poly(w["polygon"], wb.polygon)) m.water.push_back(std::move(wb));
    }
    for (const auto& l : v["bus_lines"].as_array()) {
        BusLine line;
        line.name = l.str("name", "L");
        line.color = l.str("color", "#ffd43b");
        for (const auto& n : l["path"].as_array()) line.path.push_back(static_cast<std::uint32_t>(n.as_int(0)));
        for (const auto& n : l["stops"].as_array()) line.stops.push_back(static_cast<std::uint32_t>(n.as_int(0)));
        bool ok = line.path.size() >= 2;
        for (auto n : line.path) ok &= n < m.nodes.size();
        if (ok) m.bus_lines.push_back(std::move(line));
    }
    // District centres missing in custom maps: use the mean of their buildings, else spread them out.
    for (std::size_t d = 0; d < m.districts.size(); ++d) {
        if (m.districts[d].center.x >= 0) continue;
        Vec2 sum{};
        int count = 0;
        for (const auto& b : m.buildings)
            if (b.district == static_cast<int>(d)) { sum += polygon_centroid(b.footprint); ++count; }
        m.districts[d].center = count ? sum * (1.f / static_cast<float>(count)) : Vec2{m.width / 2, m.height / 2};
    }
    return true;
}

} // namespace npc::world
