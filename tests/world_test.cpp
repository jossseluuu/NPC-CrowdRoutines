#include "core/rng.hpp"
#include "data/catalog.hpp"
#include "test_framework.hpp"
#include "world/mapgen.hpp"
#include "world/navigation.hpp"

#include <set>

using namespace npc;

namespace {
struct Fixture {
    data::Catalog cat;
    world::MapLibrary lib;
    bool ok = false;
    Fixture() {
        std::string err;
        ok = cat.load(NPC_DATA_DIR, err) && lib.load(NPC_DATA_DIR, cat, err);
        if (!ok) std::printf("  fixture error: %s\n", err.c_str());
    }
};
Fixture& fx() {
    static Fixture f;
    return f;
}

bool fully_connected(const world::CityMap& m, bool cars_only) {
    std::vector<bool> seen(m.nodes.size(), false);
    std::vector<std::uint32_t> stack{0};
    seen[0] = true;
    std::size_t count = 0;
    while (!stack.empty()) {
        const auto u = stack.back();
        stack.pop_back();
        ++count;
        for (auto ei : m.nodes[u].edges) {
            if (cars_only && !m.edges[ei].cars()) continue;
            const auto v = m.edges[ei].other(u);
            if (!seen[v]) { seen[v] = true; stack.push_back(v); }
        }
    }
    if (!cars_only) return count == m.nodes.size();
    std::size_t car_nodes = 0;
    for (const auto& n : m.nodes) {
        bool any = false;
        for (auto ei : n.edges) any |= m.edges[ei].cars();
        car_nodes += any;
    }
    return count == car_nodes;
}
} // namespace

TEST_CASE("world/every template and size generates a valid city") {
    auto& f = fx();
    REQUIRE(f.ok);
    for (const auto& t : f.lib.templates)
        for (const auto& s : f.lib.sizes) {
            world::CityMap m;
            std::string err;
            const bool ok = world::generate_map({t.id, s.id, 7, ""}, f.lib, f.cat, m, err);
            if (!ok) std::printf("  %s/%s: %s\n", t.id.c_str(), s.id.c_str(), err.c_str());
            CHECK(ok);
            if (!ok) continue;
            CHECK(fully_connected(m, false));
            CHECK(fully_connected(m, true));
            CHECK(m.buildings.size() > 40);
            CHECK(m.total_households() > static_cast<std::size_t>(s.population / 4));
            CHECK(m.total_jobs() > 0);
            CHECK(!m.bus_lines.empty());
            CHECK(m.districts.size() >= 2);
            for (const auto& [type, n] : f.cat.minimums) {
                if (m.buildings_by_type[static_cast<std::size_t>(type)].empty())
                    std::printf("  %s/%s missing required %s\n", t.id.c_str(), s.id.c_str(), f.cat.building_types[static_cast<std::size_t>(type)].id.c_str());
                CHECK(!m.buildings_by_type[static_cast<std::size_t>(type)].empty());
            }
            bool inside = true;
            for (const auto& n : m.nodes) inside &= n.pos.x >= 0 && n.pos.y >= 0 && n.pos.x <= m.width + 1 && n.pos.y <= m.height + 1;
            CHECK(inside);
            int lights = 0;
            for (const auto& n : m.nodes) lights += n.traffic_light;
            CHECK(lights > 0);
            if (t.water != "none") CHECK(!m.water.empty());
        }
}

TEST_CASE("world/generation is deterministic per seed") {
    auto& f = fx();
    REQUIRE(f.ok);
    world::CityMap a, b, c;
    std::string err;
    REQUIRE(world::generate_map({"river", "medium", 99, ""}, f.lib, f.cat, a, err));
    REQUIRE(world::generate_map({"river", "medium", 99, ""}, f.lib, f.cat, b, err));
    REQUIRE(world::generate_map({"river", "medium", 100, ""}, f.lib, f.cat, c, err));
    CHECK(a.to_json(f.cat).dump() == b.to_json(f.cat).dump());
    CHECK(a.to_json(f.cat).dump() != c.to_json(f.cat).dump());
}

TEST_CASE("world/exported map loads back identically") {
    auto& f = fx();
    REQUIRE(f.ok);
    world::CityMap a, b;
    std::string err;
    REQUIRE(world::generate_map({"coastal", "small", 3, "Puerto Prueba"}, f.lib, f.cat, a, err));
    const auto j = a.to_json(f.cat);
    REQUIRE(world::CityMap::from_json(json::parse(j.dump()).value, f.cat, b, err));
    REQUIRE(b.finalize(f.cat, err));
    CHECK(b.name == "Puerto Prueba");
    CHECK(b.nodes.size() == a.nodes.size());
    CHECK(b.edges.size() == a.edges.size());
    CHECK(b.buildings.size() == a.buildings.size());
    CHECK(b.bus_lines.size() == a.bus_lines.size());
    CHECK(b.total_households() == a.total_households());
}

TEST_CASE("world/minimal custom map gets lights, buses and capacities") {
    auto& f = fx();
    REQUIRE(f.ok);
    // 4x3 grid of streets, a few buildings given by centre + type only.
    json::Value v = json::parse(R"({
      "format": "npc-city-map", "version": 1, "name": "Aldea",
      "nodes": [[20,20],[120,20],[220,20],[320,20],[20,120],[120,120],[220,120],[320,120],[20,220],[120,220],[220,220],[320,220]],
      "edges": [[0,1,"avenue"],[1,2,"avenue"],[2,3,"avenue"],[4,5],[5,6],[6,7],[8,9],[9,10],[10,11,"path"],
                [0,4],[4,8],[1,5,"avenue"],[5,9,"avenue"],[2,6],[6,10],[3,7],[7,11]],
      "districts": [{"name": "Centro", "kind": "old_town"}],
      "buildings": [
        {"type": "apartment", "center": [70, 70]}, {"type": "house", "center": [170, 60]}, {"type": "house", "center": [180, 85]},
        {"type": "supermarket", "center": [270, 70]}, {"type": "school", "center": [70, 170]}, {"type": "cafe", "center": [170, 170]},
        {"type": "park", "center": [270, 170], "size": [70, 70]}
      ]
    })").value;
    world::CityMap m;
    std::string err;
    REQUIRE(world::CityMap::from_json(v, f.cat, m, err));
    const bool ok = m.finalize(f.cat, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    REQUIRE(ok);
    CHECK(m.buildings.size() == 7);
    CHECK(m.total_households() >= 12);
    CHECK(!m.bus_lines.empty());
    int lights = 0;
    for (const auto& n : m.nodes) lights += n.traffic_light;
    CHECK(lights > 0);
    CHECK(m.buildings[3].jobs > 0);
}

TEST_CASE("world/invalid custom maps are rejected with clear messages") {
    auto& f = fx();
    REQUIRE(f.ok);
    world::CityMap m;
    std::string err;
    CHECK(!world::CityMap::from_json(json::parse(R"({"format":"other"})").value, f.cat, m, err));
    CHECK(err.find("npc-city-map") != std::string::npos);
    CHECK(!world::CityMap::from_json(json::parse(R"({"format":"npc-city-map","nodes":[[0,0],[1,1]],"edges":[[0,5]]})").value, f.cat, m, err));
    CHECK(err.find("missing node") != std::string::npos);
    CHECK(!world::CityMap::from_json(json::parse(R"({"format":"npc-city-map","nodes":[[0,0],[9,9]],"edges":[[0,1,"highway"]]})").value, f.cat, m, err));
    CHECK(err.find("highway") != std::string::npos);
    CHECK(!world::CityMap::from_json(json::parse(R"({"format":"npc-city-map","nodes":[[0,0],[9,9]],"edges":[[0,1]],"buildings":[{"type":"castle","center":[1,1]}]})").value, f.cat, m, err));
    CHECK(err.find("castle") != std::string::npos);
}

TEST_CASE("world/router finds routes and respects closures") {
    auto& f = fx();
    REQUIRE(f.ok);
    world::CityMap m;
    std::string err;
    REQUIRE(world::generate_map({"grid", "small", 11, ""}, f.lib, f.cat, m, err));
    world::Router router(m);
    Rng rng(5);
    int ok_walk = 0, ok_car = 0;
    for (int i = 0; i < 200; ++i) {
        const auto a = rng.below(static_cast<std::uint32_t>(m.nodes.size()));
        const auto b = rng.below(static_cast<std::uint32_t>(m.nodes.size()));
        world::Route r;
        if (router.route(a, b, world::TravelMode::Walk, r)) {
            ++ok_walk;
            // The route is a valid chain from a to b.
            auto n = a;
            bool chain = true;
            for (auto e : *r.edges) {
                chain &= m.edges[e].a == n || m.edges[e].b == n;
                n = m.edges[e].other(n);
            }
            CHECK(chain && n == b);
        }
        world::Route rc;
        const auto ca = m.nearest_node_with_cars(m.nodes[a].pos), cb = m.nearest_node_with_cars(m.nodes[b].pos);
        if (router.route(ca, cb, world::TravelMode::Car, rc)) {
            ++ok_car;
            bool cars = true;
            for (auto e : *rc.edges) cars &= m.edges[e].cars();
            CHECK(cars);
        }
    }
    CHECK(ok_walk == 200);
    CHECK(ok_car == 200);

    // Close every edge of a route for cars: the new car route must avoid them.
    const auto a = m.nearest_node_with_cars({20, 20}), b = m.nearest_node_with_cars({m.width - 20, m.height - 20});
    world::Route first;
    REQUIRE(router.route(a, b, world::TravelMode::Car, first));
    const auto blocked = (*first.edges)[first.size() / 2];
    router.set_car_blocked(blocked, true);
    world::Route second;
    REQUIRE(router.route(a, b, world::TravelMode::Car, second));
    CHECK(std::find(second.edges->begin(), second.edges->end(), blocked) == second.edges->end());
    // Pedestrians may still use it.
    router.clear_closures();
    world::Route again;
    REQUIRE(router.route(a, b, world::TravelMode::Car, again));
    CHECK(again.size() == first.size());
}
