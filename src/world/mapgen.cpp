#include "world/mapgen.hpp"

#include "core/rng.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace npc::world {

// ---------------------------------------------------------------------------------------------
// Library
// ---------------------------------------------------------------------------------------------
bool MapLibrary::load(const std::string& dir, const data::Catalog& cat, std::string& err) {
    templates.clear();
    sizes.clear();
    auto r = json::parse_file(dir + "/maps/templates.json");
    if (!r.ok()) { err = r.error; return false; }
    for (const auto& [id, s] : r.value["sizes"].as_object())
        sizes.push_back({id, s.flt("width", 1000), s.flt("height", 800), static_cast<int>(s.integer("population", 3000))});
    for (const auto& t : r.value["templates"].as_array()) {
        MapTemplate mt;
        mt.id = t.str("id");
        const std::string ctx = "templates.json '" + mt.id + "'";
        const auto& l = t["layout"];
        mt.spacing[0] = std::max(30.f, l["spacing"][0].as_float(80));
        mt.spacing[1] = std::max(30.f, l["spacing"][1].as_float(70));
        mt.jitter = std::clamp(l.flt("jitter", 0), 0.f, 0.4f);
        mt.warp = std::max(0.f, l.flt("warp", 0));
        mt.avenue_every = static_cast<int>(l.integer("avenue_every", 0));
        mt.diagonal = l.boolean("diagonal", false);
        mt.remove = std::clamp(l.flt("remove", 0), 0.f, 0.5f);
        mt.path_ratio = std::clamp(l.flt("path_ratio", 0.5f), 0.f, 1.f);
        const auto& w = t["water"];
        mt.water = w.str("type", "none");
        if (mt.water != "none" && mt.water != "river" && mt.water != "coast") { err = ctx + ": water type must be none, river or coast"; return false; }
        mt.water_width = w.flt("width", 60);
        mt.water_amplitude = w.flt("amplitude", 50);
        mt.water_level = std::clamp(w.flt("level", 0.78f), 0.4f, 0.95f);
        mt.bridge_every = std::max(1, static_cast<int>(w.integer("bridge_every", 3)));
        for (const auto& z : t["zoning"].as_array()) {
            ZoningRule rule;
            rule.chance = z.flt("chance", 1);
            if (z.contains("default")) {
                rule.shape = ZoningRule::Shape::Default;
                for (const auto& [k, wt] : z["default"].as_object()) {
                    const int di = cat.district_index(k);
                    if (di < 0) { err = ctx + ": unknown district '" + k + "'"; return false; }
                    rule.weights.emplace_back(di, wt.as_float(1));
                }
            } else {
                rule.district = cat.district_index(z.str("district"));
                if (rule.district < 0) { err = ctx + ": unknown district '" + z.str("district") + "'"; return false; }
                if (z.contains("circle")) {
                    rule.shape = ZoningRule::Shape::Circle;
                    rule.a = z["circle"][0].as_float(); rule.b = z["circle"][1].as_float(); rule.c = z["circle"][2].as_float(0.1f);
                } else if (z.contains("rect")) {
                    rule.shape = ZoningRule::Shape::Rect;
                    rule.a = z["rect"][0].as_float(); rule.b = z["rect"][1].as_float();
                    rule.c = z["rect"][2].as_float(1); rule.d = z["rect"][3].as_float(1);
                } else if (z.contains("band")) {
                    rule.shape = z.str("band") == "coast" ? ZoningRule::Shape::BandCoast : ZoningRule::Shape::BandRiver;
                    rule.a = z.flt("width", 60);
                } else {
                    err = ctx + ": zoning rule needs circle, rect, band or default";
                    return false;
                }
            }
            mt.zoning.push_back(std::move(rule));
        }
        templates.push_back(std::move(mt));
    }
    if (templates.empty() || sizes.empty()) { err = "templates.json: needs templates and sizes"; return false; }

    auto n = json::parse_file(dir + "/maps/names.json");
    if (!n.ok()) { err = n.error; return false; }
    for (const auto& s : n.value["streets"].as_array()) names.streets.push_back(s.as_string());
    for (const auto& s : n.value["street_prefix"].as_array()) names.prefixes.push_back(s.as_string());
    for (const auto& s : n.value["districts"].as_array()) names.districts.push_back(s.as_string());
    if (names.streets.empty()) names.streets = {"Mayor"};
    if (names.prefixes.empty()) names.prefixes = {"Calle"};
    if (names.districts.empty()) names.districts = {"Centro"};
    return true;
}

const MapTemplate* MapLibrary::find_template(std::string_view id) const {
    for (const auto& t : templates)
        if (t.id == id) return &t;
    return nullptr;
}
const MapSize* MapLibrary::find_size(std::string_view id) const {
    for (const auto& s : sizes)
        if (s.id == id) return &s;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Generator
// ---------------------------------------------------------------------------------------------
namespace {

constexpr std::uint32_t kDead = std::numeric_limits<std::uint32_t>::max();

struct Water {
    std::string type;
    float width = 0, amp = 0, level = 0, phase = 0, freq = 0, W = 0, H = 0;
    float river_y(float x) const { return H * 0.5f + amp * std::sin(freq * x + phase); }
    float coast_y(float x) const { return H * level + amp * std::sin(freq * x + phase) * 0.6f + amp * 0.4f * std::sin(freq * 2.3f * x + phase * 1.7f); }
    bool wet(Vec2 p, float margin) const {
        if (type == "river") return std::abs(p.y - river_y(p.x)) < width * 0.5f + margin;
        if (type == "coast") return p.y > coast_y(p.x) - margin;
        return false;
    }
    float distance_to(Vec2 p) const {
        if (type == "river") return std::max(0.f, std::abs(p.y - river_y(p.x)) - width * 0.5f);
        if (type == "coast") return std::max(0.f, coast_y(p.x) - p.y);
        return 1e9f;
    }
};

bool car_connected(const CityMap& m, const std::vector<bool>& edge_alive) {
    std::vector<std::vector<std::uint32_t>> adj(m.nodes.size());
    std::size_t start = m.nodes.size();
    for (std::uint32_t i = 0; i < m.edges.size(); ++i) {
        if (!edge_alive[i] || !m.edges[i].cars()) continue;
        adj[m.edges[i].a].push_back(m.edges[i].b);
        adj[m.edges[i].b].push_back(m.edges[i].a);
        start = std::min<std::size_t>(start, m.edges[i].a);
    }
    if (start == m.nodes.size()) return false;
    std::vector<bool> seen(m.nodes.size(), false);
    std::vector<std::uint32_t> stack{static_cast<std::uint32_t>(start)};
    seen[start] = true;
    std::size_t reached = 0, with_edges = 0;
    while (!stack.empty()) {
        const auto u = stack.back();
        stack.pop_back();
        ++reached;
        for (auto v : adj[u])
            if (!seen[v]) { seen[v] = true; stack.push_back(v); }
    }
    for (const auto& a : adj) with_edges += !a.empty();
    return reached == with_edges;
}

Vec2 bilinear(const Polygon& q, float u, float v) {
    return lerp(lerp(q[0], q[1], u), lerp(q[3], q[2], u), v);
}

} // namespace

bool generate_map(const MapSpec& spec, const MapLibrary& lib, const data::Catalog& cat, CityMap& m, std::string& err) {
    const MapTemplate* tpl = lib.find_template(spec.template_id);
    if (!tpl) { err = "unknown map template '" + spec.template_id + "'"; return false; }
    const MapSize* size = lib.find_size(spec.size_id);
    if (!size) { err = "unknown map size '" + spec.size_id + "'"; return false; }

    m = CityMap{};
    m.template_id = tpl->id;
    m.size_id = size->id;
    m.seed = spec.seed;
    m.width = size->width;
    m.height = size->height;
    m.suggested_population = size->population;
    Rng rng(spec.seed, "mapgen:" + tpl->id);
    const float W = size->width, H = size->height;

    Water water;
    water.type = tpl->water;
    water.width = tpl->water_width;
    water.amp = tpl->water_amplitude;
    water.level = tpl->water_level;
    water.phase = rng.range(0.f, 6.28f);
    water.freq = 6.2831853f / (W * rng.range(0.7f, 1.1f));
    water.W = W;
    water.H = H;

    // 1. Deformed lattice of intersections.
    const float margin = 18.f;
    const int nx = std::max(3, static_cast<int>((W - 2 * margin) / tpl->spacing[0]) + 1);
    const int ny = std::max(3, static_cast<int>((H - 2 * margin) / tpl->spacing[1]) + 1);
    const float sx = (W - 2 * margin) / static_cast<float>(nx - 1), sy = (H - 2 * margin) / static_cast<float>(ny - 1);
    const float wp[4] = {rng.range(0.f, 6.28f), rng.range(0.f, 6.28f), rng.range(0.f, 6.28f), rng.range(0.f, 6.28f)};
    std::vector<std::uint32_t> lattice(static_cast<std::size_t>(nx * ny), kDead);
    auto lat = [&](int i, int j) -> std::uint32_t& { return lattice[static_cast<std::size_t>(j * nx + i)]; };
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            Vec2 p{margin + static_cast<float>(i) * sx, margin + static_cast<float>(j) * sy};
            const bool border = i == 0 || j == 0 || i == nx - 1 || j == ny - 1;
            if (!border) {
                p.x += rng.range(-tpl->jitter, tpl->jitter) * sx;
                p.y += rng.range(-tpl->jitter, tpl->jitter) * sy;
            }
            if (tpl->warp > 0) {
                const float k = border ? 0.35f : 1.f;
                p.x += k * tpl->warp * 0.5f * (std::sin(p.y * 0.006f + wp[0]) + std::sin(p.x * 0.004f + wp[1]));
                p.y += k * tpl->warp * 0.5f * (std::sin(p.x * 0.005f + wp[2]) + std::sin(p.y * 0.007f + wp[3]));
            }
            p.x = std::clamp(p.x, 4.f, W - 4.f);
            p.y = std::clamp(p.y, 4.f, H - 4.f);
            if (water.wet(p, 10.f)) continue;
            lat(i, j) = static_cast<std::uint32_t>(m.nodes.size());
            m.nodes.push_back({p, {}, false});
        }

    // 2. Street names per row/column.
    auto pick_name = [&](std::vector<bool>& used) {
        for (int tries = 0; tries < 40; ++tries) {
            const auto k = rng.below(static_cast<std::uint32_t>(lib.names.streets.size()));
            if (!used[k] || tries == 39) { used[k] = true; return lib.names.streets[k]; }
        }
        return lib.names.streets[0];
    };
    std::vector<bool> used_names(lib.names.streets.size(), false);
    std::vector<int> row_street(static_cast<std::size_t>(ny)), col_street(static_cast<std::size_t>(nx));
    auto add_street = [&](const std::string& prefix, const std::string& base) {
        m.street_names.push_back(prefix + " " + base);
        return static_cast<int>(m.street_names.size()) - 1;
    };
    for (int j = 0; j < ny; ++j) {
        const bool avenue = tpl->avenue_every > 0 && j % tpl->avenue_every == 0;
        row_street[static_cast<std::size_t>(j)] = add_street(avenue ? "Avenida" : lib.names.prefixes[rng.below(static_cast<std::uint32_t>(lib.names.prefixes.size()))], pick_name(used_names));
    }
    for (int i = 0; i < nx; ++i) {
        const bool avenue = tpl->avenue_every > 0 && i % tpl->avenue_every == 0;
        col_street[static_cast<std::size_t>(i)] = add_street(avenue ? "Avenida" : lib.names.prefixes[rng.below(static_cast<std::uint32_t>(lib.names.prefixes.size()))], pick_name(used_names));
    }

    // 3. Edges.
    auto add_edge = [&](std::uint32_t a, std::uint32_t b, RoadKind kind, int street) {
        if (a == kDead || b == kDead) return;
        Edge e;
        e.a = a;
        e.b = b;
        e.kind = kind;
        e.street = street;
        m.edges.push_back(e);
    };
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i + 1 < nx; ++i) {
            const bool avenue = tpl->avenue_every > 0 && j % tpl->avenue_every == 0;
            add_edge(lat(i, j), lat(i + 1, j), avenue ? RoadKind::Avenue : RoadKind::Street, row_street[static_cast<std::size_t>(j)]);
        }
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j + 1 < ny; ++j) {
            const bool avenue = tpl->avenue_every > 0 && i % tpl->avenue_every == 0;
            add_edge(lat(i, j), lat(i, j + 1), avenue ? RoadKind::Avenue : RoadKind::Street, col_street[static_cast<std::size_t>(i)]);
        }
    // Bridges across the river.
    if (water.type == "river") {
        for (int i = 1; i + 1 < nx; ++i) {
            if (i % tpl->bridge_every != 0) continue;
            for (int j = 0; j + 1 < ny; ++j) {
                if (lat(i, j) == kDead || lat(i, j + 1) != kDead) continue;
                int k = j + 1;
                while (k < ny && lat(i, k) == kDead) ++k;
                if (k < ny && k - j <= 4) {
                    const int name = add_street("Puente", lib.names.streets[rng.below(static_cast<std::uint32_t>(lib.names.streets.size()))]);
                    add_edge(lat(i, j), lat(i, k), RoadKind::Bridge, name);
                }
                break;
            }
        }
    }
    // Coastal promenade: lowest surviving node of each column joined along the shore.
    if (water.type == "coast") {
        const int name = add_street("Paseo", "Marítimo");
        std::uint32_t prev = kDead;
        for (int i = 0; i < nx; ++i) {
            std::uint32_t low = kDead;
            for (int j = ny - 1; j >= 0; --j)
                if (lat(i, j) != kDead) { low = lat(i, j); break; }
            if (prev != kDead && low != kDead) {
                bool exists = false;
                for (const auto& e : m.edges) exists |= (e.a == prev && e.b == low) || (e.a == low && e.b == prev);
                if (!exists) add_edge(prev, low, RoadKind::Promenade, name);
                else for (auto& e : m.edges) if ((e.a == prev && e.b == low) || (e.a == low && e.b == prev)) { e.kind = RoadKind::Promenade; e.street = name; }
            }
            prev = low;
        }
    }
    // Diagonal avenue.
    std::vector<bool> diagonal_cell(static_cast<std::size_t>(nx * ny), false);
    if (tpl->diagonal) {
        const int name = add_street("Avenida", "Diagonal");
        const int steps = std::min(nx, ny) - 1;
        const int ox = rng.below(static_cast<std::uint32_t>(std::max(1, nx - steps)));
        for (int k = 0; k < steps; ++k) {
            const int i = ox + k, j = k;
            if (i + 1 >= nx || j + 1 >= ny) break;
            if (lat(i, j) == kDead || lat(i + 1, j + 1) == kDead) continue;
            add_edge(lat(i, j), lat(i + 1, j + 1), RoadKind::Avenue, name);
            diagonal_cell[static_cast<std::size_t>(j * nx + i)] = true;
        }
    }

    // 4. Prune streets (organic look) while keeping the car network connected.
    {
        std::vector<bool> alive(m.edges.size(), true);
        std::vector<std::uint32_t> order(m.edges.size());
        for (std::uint32_t i = 0; i < order.size(); ++i) order[i] = i;
        for (std::size_t i = order.size(); i > 1; --i) std::swap(order[i - 1], order[rng.below(static_cast<std::uint32_t>(i))]);
        for (auto ei : order) {
            auto& e = m.edges[ei];
            if (e.kind != RoadKind::Street || !rng.chance(tpl->remove)) continue;
            const RoadKind old = e.kind;
            const bool keep_as_path = rng.chance(tpl->path_ratio);
            if (keep_as_path) e.kind = RoadKind::Path; else alive[ei] = false;
            if (!car_connected(m, alive)) { e.kind = old; alive[ei] = true; }
        }
        std::vector<Edge> kept;
        for (std::uint32_t i = 0; i < m.edges.size(); ++i)
            if (alive[i]) kept.push_back(m.edges[i]);
        m.edges = std::move(kept);
    }

    // 5. Neighbourhoods (Voronoi seeds on a jittered grid).
    const int seeds_x = std::max(2, static_cast<int>(std::round(W / 330.f)));
    const int seeds_y = std::max(2, static_cast<int>(std::round(H / 330.f)));
    std::vector<std::string> district_names = lib.names.districts;
    for (std::size_t i = district_names.size(); i > 1; --i) std::swap(district_names[i - 1], district_names[rng.below(static_cast<std::uint32_t>(i))]);
    for (int j = 0; j < seeds_y; ++j)
        for (int i = 0; i < seeds_x; ++i) {
            District d;
            d.center = {(static_cast<float>(i) + rng.range(0.25f, 0.75f)) * W / static_cast<float>(seeds_x),
                        (static_cast<float>(j) + rng.range(0.25f, 0.75f)) * H / static_cast<float>(seeds_y)};
            if (water.wet(d.center, 0)) continue;
            const std::size_t k = m.districts.size();
            d.name = k < district_names.size() ? district_names[k] : district_names[k % district_names.size()] + " " + std::to_string(k / district_names.size() + 1);
            m.districts.push_back(d);
        }
    if (m.districts.empty()) m.districts.push_back({"Centro", 0, {W / 2, H / 2}});

    // 6. Blocks from lattice cells (split by the diagonal avenue).
    auto zone_kind = [&](Vec2 c) {
        const float u = c.x / W, v = c.y / H, mn = std::min(W, H);
        for (const auto& r : tpl->zoning) {
            if (r.chance < 1.f && !rng.chance(r.chance)) continue;
            switch (r.shape) {
                case ZoningRule::Shape::Circle:
                    if (length_sq({(u - r.a) * W, (v - r.b) * H}) <= (r.c * mn) * (r.c * mn)) return r.district;
                    break;
                case ZoningRule::Shape::Rect:
                    if (u >= r.a && u <= r.c && v >= r.b && v <= r.d) return r.district;
                    break;
                case ZoningRule::Shape::BandRiver:
                    if (water.type == "river" && water.distance_to(c) <= r.a) return r.district;
                    break;
                case ZoningRule::Shape::BandCoast:
                    if (water.type == "coast" && water.distance_to(c) <= r.a) return r.district;
                    break;
                case ZoningRule::Shape::Default: {
                    std::vector<float> w;
                    for (const auto& [k, wt] : r.weights) w.push_back(wt);
                    return r.weights[rng.weighted(w)].first;
                }
            }
        }
        return std::max(0, cat.district_index("residential_mid"));
    };
    for (int j = 0; j + 1 < ny; ++j)
        for (int i = 0; i + 1 < nx; ++i) {
            const std::uint32_t c00 = lat(i, j), c10 = lat(i + 1, j), c11 = lat(i + 1, j + 1), c01 = lat(i, j + 1);
            if (c00 == kDead || c10 == kDead || c11 == kDead || c01 == kDead) continue;
            const Polygon quad{m.nodes[c00].pos, m.nodes[c10].pos, m.nodes[c11].pos, m.nodes[c01].pos};
            if (water.wet(polygon_centroid(quad), 4.f)) continue;
            std::vector<Polygon> parts;
            if (diagonal_cell[static_cast<std::size_t>(j * nx + i)]) {
                parts.push_back(inset({quad[0], quad[1], quad[2]}, 9.f));
                parts.push_back(inset({quad[0], quad[2], quad[3]}, 9.f));
            } else {
                parts.push_back(inset(quad, 7.f));
            }
            for (auto& p : parts) {
                Block b;
                b.polygon = std::move(p);
                const Vec2 c = polygon_centroid(b.polygon);
                b.district = m.district_at(c);
                b.kind = zone_kind(c);
                m.blocks.push_back(std::move(b));
            }
        }
    // Dominant land use per neighbourhood.
    {
        std::vector<std::map<int, int>> votes(m.districts.size());
        for (const auto& b : m.blocks) ++votes[static_cast<std::size_t>(b.district)][b.kind];
        for (std::size_t d = 0; d < m.districts.size(); ++d) {
            int best = std::max(0, cat.district_index("residential_mid")), best_n = -1;
            for (const auto& [k, n] : votes[d])
                if (n > best_n) { best = k; best_n = n; }
            m.districts[d].kind = best;
        }
    }

    // 7. Buildings.
    auto type_size = [&](int t) {
        const auto& bt = cat.building_types[static_cast<std::size_t>(t)];
        return Vec2{(bt.size[0] + bt.size[1]) * 0.5f, (bt.size[2] + bt.size[3]) * 0.5f};
    };
    for (const auto& block : m.blocks) {
        const auto& kind = cat.districts[static_cast<std::size_t>(block.kind)];
        std::vector<float> weights;
        for (const auto& [t, w] : kind.weights) weights.push_back(w);
        const Polygon& q = block.polygon;
        const float bw = q.size() == 4 ? (distance(q[0], q[1]) + distance(q[3], q[2])) * 0.5f : std::sqrt(polygon_area(q));
        const float bh = q.size() == 4 ? (distance(q[0], q[3]) + distance(q[1], q[2])) * 0.5f : std::sqrt(polygon_area(q));
        const float bmin = std::min(bw, bh);
        const int first = kind.weights[rng.weighted(weights)].first;
        const auto& fbt = cat.building_types[static_cast<std::size_t>(first)];
        auto make_building = [&](int type, Polygon fp) {
            Building b;
            b.type = type;
            b.footprint = std::move(fp);
            b.district = block.district;
            const auto& bt = cat.building_types[static_cast<std::size_t>(type)];
            b.floors = rng.between(bt.floors[0], std::max(bt.floors[0], bt.floors[1]));
            m.buildings.push_back(std::move(b));
        };
        // Large types (parks, malls, campuses...) or triangles take the whole block.
        if (q.size() != 4 || std::min(fbt.size[0], fbt.size[2]) > 0.5f * bmin || fbt.floors[1] == 0) {
            make_building(first, inset(q, fbt.floors[1] == 0 ? 1.f : 3.f));
            continue;
        }
        // Otherwise subdivide into lots sized after the district's typical small building.
        Vec2 typical{};
        float tw = 0;
        for (const auto& [t, w] : kind.weights) {
            const auto& bt = cat.building_types[static_cast<std::size_t>(t)];
            if (std::min(bt.size[0], bt.size[2]) > 0.5f * bmin || bt.floors[1] == 0) continue;
            typical += type_size(t) * w;
            tw += w;
        }
        if (tw <= 0) { make_building(first, inset(q, 3.f)); continue; }
        typical = typical * (1.f / tw);
        const int na = std::clamp(static_cast<int>(std::round(bw / (typical.x + 6.f))), 1, 8);
        const int nb = std::clamp(static_cast<int>(std::round(bh / (typical.y + 6.f))), 1, 8);
        for (int a = 0; a < na; ++a)
            for (int b = 0; b < nb; ++b) {
                // Interior lots of big blocks become courtyards/yards with probability depending on density.
                const bool interior = a > 0 && b > 0 && a + 1 < na && b + 1 < nb;
                if (rng.chance(interior ? 0.85f : (1.f - kind.density) * 0.55f)) continue;
                int type = kind.weights[rng.weighted(weights)].first;
                const float lw = bw / static_cast<float>(na), lh = bh / static_cast<float>(nb);
                const auto& bt = cat.building_types[static_cast<std::size_t>(type)];
                if (bt.size[0] > lw * 1.25f || bt.size[2] > lh * 1.25f || bt.floors[1] == 0) {
                    // Too big for the lot: fall back to the most likely type that fits.
                    int best = -1;
                    float best_w = -1;
                    for (const auto& [t, w] : kind.weights) {
                        const auto& c = cat.building_types[static_cast<std::size_t>(t)];
                        if (c.size[0] <= lw * 1.25f && c.size[2] <= lh * 1.25f && c.floors[1] > 0 && w > best_w) { best = t; best_w = w; }
                    }
                    if (best < 0) continue;
                    type = best;
                }
                const auto& ft = cat.building_types[static_cast<std::size_t>(type)];
                const float fw = std::min(lw - 3.f, rng.range(ft.size[0], ft.size[1])) / lw;
                const float fh = std::min(lh - 3.f, rng.range(ft.size[2], ft.size[3])) / lh;
                if (fw <= 0.1f || fh <= 0.1f) continue;
                // Buildings face the street: anchor towards the block edge they belong to.
                const float u0 = static_cast<float>(a) / static_cast<float>(na), v0 = static_cast<float>(b) / static_cast<float>(nb);
                const float du = 1.f / static_cast<float>(na), dv = 1.f / static_cast<float>(nb);
                const float cu = a == 0 ? u0 + du * fw * 0.5f + 0.01f : (a + 1 == na ? u0 + du * (1 - fw * 0.5f) - 0.01f : u0 + du * 0.5f);
                const float cv = b == 0 ? v0 + dv * fh * 0.5f + 0.01f : (b + 1 == nb ? v0 + dv * (1 - fh * 0.5f) - 0.01f : v0 + dv * 0.5f);
                const float hu = du * fw * 0.5f, hv = dv * fh * 0.5f;
                make_building(type, {bilinear(q, cu - hu, cv - hv), bilinear(q, cu + hu, cv - hv), bilinear(q, cu + hu, cv + hv), bilinear(q, cu - hu, cv + hv)});
            }
    }

    // 8. Guarantee essential services (scaled with city size).
    const float scale = std::max(1.f, W * H / (900.f * 650.f) * 0.5f);
    for (const auto& [type, minimum] : cat.minimums) {
        int need = static_cast<int>(std::ceil(static_cast<float>(minimum) * (minimum > 1 ? scale : std::sqrt(scale))));
        int have = 0;
        for (const auto& b : m.buildings) have += b.type == type;
        const Vec2 ts = type_size(type);
        const float want_area = ts.x * ts.y * 0.45f;
        while (have < need) {
            // Replace the largest building whose type is over-represented and isn't itself a required service.
            int best = -1;
            float best_area = 0;
            for (std::size_t i = 0; i < m.buildings.size(); ++i) {
                const auto& b = m.buildings[i];
                bool protected_type = false;
                for (const auto& [mt, mn] : cat.minimums) protected_type |= mt == b.type;
                if (protected_type) continue;
                const float area = polygon_area(b.footprint) * (0.9f + 0.2f * rng.uniformf());
                if (area > best_area && area >= want_area * 0.35f) { best_area = area; best = static_cast<int>(i); }
            }
            if (best < 0) {
                // Nothing big enough: clear the largest block without required services and build there.
                int best_block = -1;
                float block_area = 0;
                for (std::size_t bi = 0; bi < m.blocks.size(); ++bi) {
                    const auto& blk = m.blocks[bi];
                    bool has_protected = false;
                    for (const auto& b : m.buildings) {
                        if (!point_in_polygon(blk.polygon, polygon_centroid(b.footprint))) continue;
                        for (const auto& [mt, mn] : cat.minimums) has_protected |= mt == b.type;
                    }
                    const float area = polygon_area(blk.polygon);
                    if (!has_protected && area > block_area) { block_area = area; best_block = static_cast<int>(bi); }
                }
                if (best_block < 0) break;
                const auto& blk = m.blocks[static_cast<std::size_t>(best_block)];
                std::erase_if(m.buildings, [&](const Building& b) { return point_in_polygon(blk.polygon, polygon_centroid(b.footprint)); });
                Building nb;
                nb.footprint = inset(blk.polygon, 3.f);
                nb.district = blk.district;
                m.buildings.push_back(std::move(nb));
                best = static_cast<int>(m.buildings.size()) - 1;
            }
            auto& b = m.buildings[static_cast<std::size_t>(best)];
            b.type = type;
            const auto& bt = cat.building_types[static_cast<std::size_t>(type)];
            b.floors = rng.between(bt.floors[0], std::max(bt.floors[0], bt.floors[1]));
            ++have;
        }
    }

    // 9. Water polygons for rendering.
    if (water.type == "river") {
        WaterBody w;
        w.kind = "river";
        for (float x = -10; x <= W + 10; x += 20) w.polygon.push_back({x, water.river_y(x) - water.width * 0.5f});
        for (float x = W + 10; x >= -10; x -= 20) w.polygon.push_back({x, water.river_y(x) + water.width * 0.5f});
        m.water.push_back(std::move(w));
    } else if (water.type == "coast") {
        WaterBody w;
        w.kind = "sea";
        for (float x = -10; x <= W + 10; x += 20) w.polygon.push_back({x, water.coast_y(x)});
        w.polygon.push_back({W + 10, H + 10});
        w.polygon.push_back({-10, H + 10});
        m.water.push_back(std::move(w));
    }

    m.name = spec.name.empty() ? (m.districts.front().name + " (" + tpl->id + ", " + size->id + ")") : spec.name;
    return m.finalize(cat, err);
}

} // namespace npc::world
