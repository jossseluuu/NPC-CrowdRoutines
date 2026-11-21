// Pruebas de los sistemas nativos (personalidad, edificios, tráfico).
// Ejecutar: ctest -C Release   o directamente   build/Release/systems_test.exe

#include "personality/archetype.hpp"
#include "simulation/systems/building_system.hpp"
#include "simulation/systems/traffic_system.hpp"
#include "world/sidewalk.hpp"
#include "world/zone.hpp"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

using namespace npc_crowd;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            ++g_failures;                                                              \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);              \
        }                                                                              \
    } while (0)

// ---------------------------------------------------------------------------------------------
static void test_personality() {
    std::puts("personality");
    using namespace personality;
    const auto all = get_all_archetypes();
    CHECK(all.size() == 11);

    std::set<std::string> names;
    std::set<float> risk_values;
    for (const auto* a : all) {
        names.insert(a->name);
        risk_values.insert(a->risk_aversion);
        CHECK(a->traits.size() == 18);
    }
    CHECK(names.size() == 11);         // nombres únicos
    CHECK(risk_values.size() >= 8);    // los arquetipos ya no son todos iguales

    CHECK(get_risk_seeker_archetype().risk_aversion < get_anxious_archetype().risk_aversion);
    CHECK(get_extrovert_archetype().sociability > get_introvert_archetype().sociability);
    CHECK(std::abs(get_extrovert_archetype().get_trait_value("Extraversion") - get_extrovert_archetype().sociability) < 1e-6f);

    register_archetype(PersonalityArchetype("Custom"));
    CHECK(get_all_archetypes().size() == 12);
    CHECK(get_all_archetypes().back()->name == "Custom");

    std::mt19937 rng(7);
    auto r = PersonalityArchetype::random_archetype(rng);
    CHECK(r.traits.size() == 18); // antes se añadían 20 rasgos extra
    for (int i = 0; i < 200; ++i) r.mutate(1.f, rng);
    bool in_range = true;
    for (const auto& t : r.traits) in_range &= t.value >= 0.f && t.value <= 1.f;
    in_range &= r.risk_aversion >= 0.f && r.risk_aversion <= 1.f;
    CHECK(in_range);

    // La mutación es simétrica: la media de muchos arquetipos mutados se mantiene cerca de 0.5.
    float sum = 0.f;
    for (int i = 0; i < 400; ++i) {
        PersonalityArchetype p("m");
        for (int k = 0; k < 20; ++k) p.mutate(1.f, rng);
        sum += p.sociability;
    }
    CHECK(std::abs(sum / 400.f - 0.5f) < 0.08f);

    auto child = PersonalityArchetype::crossover(get_student_archetype(), get_retired_archetype(), rng);
    CHECK(std::abs(child.patience - (get_student_archetype().patience + get_retired_archetype().patience) / 2) < 1e-6f);

    Agent agent;
    agent.risk_aversion = 0.123f;
    get_entrepreneur_archetype().apply_to_agent(agent);
    CHECK(agent.risk_aversion == get_entrepreneur_archetype().risk_aversion); // ya no se pisa con la agresividad
    CHECK(agent.curiosity == get_entrepreneur_archetype().curiosity);
}

// ---------------------------------------------------------------------------------------------
static world::Building make_building(world::BuildingType type, Vec2 pos, std::uint32_t capacity) {
    const auto& cfg = world::BuildingTypeConfig::get(type);
    world::Building b;
    b.type = type;
    b.name = cfg.name;
    b.position = pos;
    b.max_capacity = capacity;
    b.hours = cfg.default_hours;
    b.is_24h = cfg.is_24h;
    return b;
}

static void test_opening_hours() {
    std::puts("opening hours");
    const auto& office = world::BuildingTypeConfig::get(world::BuildingType::OfficeSmall).default_hours;
    CHECK(!office.is_open(7.f, 0, 10));  // lunes 07:00
    CHECK(office.is_open(9.f, 0, 10));   // lunes 09:00
    CHECK(!office.is_open(12.f, 5, 15)); // sábado
    CHECK(std::abs(office.time_until_open(7.f, 0, 10) - 1.f) < 0.26f);
    CHECK(std::abs(office.time_until_close(17.f, 0, 10) - 2.f) < 0.26f);

    const auto& bar = world::BuildingTypeConfig::get(world::BuildingType::Bar).default_hours; // 20:00 - 03:00
    CHECK(bar.is_open(22.f, 2, 10));
    CHECK(bar.is_open(1.f, 3, 11));      // pasada la medianoche
    CHECK(!bar.is_open(12.f, 3, 11));

    world::OpeningHours lunch_break = office;
    lunch_break.days[0].breaks.push_back({14.f, 15.f});
    CHECK(!lunch_break.is_open(14.5f, 0, 10));
    CHECK(lunch_break.is_open(15.5f, 0, 10));
}

static void test_buildings() {
    std::puts("buildings");
    world::BuildingRegistry registry;
    world::ZoneRegistry zones;

    auto office = make_building(world::BuildingType::OfficeMedium, {100, 100}, 2);
    world::InteriorZone server_room;
    server_room.id = 7;
    server_room.capacity = 1;
    server_room.is_restricted = true;
    office.zones.push_back(server_room);
    const auto office_id = registry.add(std::move(office));
    const auto hospital_id = registry.add(make_building(world::BuildingType::Hospital, {400, 400}, 100));
    CHECK(registry.find_by_type(world::BuildingType::Hospital).size() == 1);

    world::Zone district;
    district.id = 1;
    district.building_ids = {office_id, hospital_id};
    zones.zones.emplace(1, district);

    using R = simulation::systems::BuildingOperationResult;
    simulation::systems::BuildingSystem sys(registry, zones);
    sys.set_clock(7.f, 0, 10);
    sys.initialize();
    CHECK(sys.stats.total_buildings == 2);

    CHECK(sys.enter_building(1, office_id) == R::Closed); // 07:00, oficina cerrada
    CHECK(sys.enter_building(1, 999) == R::BuildingNotFound);
    sys.set_clock(9.f, 0, 10);
    CHECK(sys.enter_building(3, office_id, 42) == R::InvalidZone);
    CHECK(sys.enter_building(1, office_id) == R::Success);
    CHECK(sys.enter_building(2, office_id, 7) == R::Restricted);
    CHECK(sys.enter_building(2, office_id, 7, true) == R::Success);
    CHECK(sys.get_zone_for_agent(2) == 7);
    CHECK(sys.enter_building(3, office_id) == R::AtCapacity);
    CHECK(sys.get_occupancy(office_id).current_occupancy == 2);
    CHECK(std::abs(sys.get_occupancy(office_id).occupancy_ratio - 1.f) < 1e-6f);

    // Cambiar de edificio saca al agente del anterior.
    CHECK(sys.enter_building(1, hospital_id) == R::Success);
    CHECK(sys.get_building_for_agent(1) == hospital_id);
    CHECK(sys.get_occupancy(office_id).current_occupancy == 1);

    CHECK(sys.exit_building(2, hospital_id) == R::AgentNotFound);
    CHECK(sys.exit_building(2, office_id) == R::Success);
    CHECK(sys.get_occupancy(office_id).zone_occupancy.at(7) == 0);
    CHECK(sys.get_building_for_agent(2) == 0);

    CHECK(sys.find_open_hospitals().size() == 1);
    CHECK(sys.find_open_offices().size() == 1);
    sys.set_clock(3.f, 0, 10);
    CHECK(sys.find_open_offices().empty());     // de madrugada
    CHECK(sys.find_open_hospitals().size() == 1); // 24h
    CHECK(sys.find_buildings_in_zone(1).size() == 2);
    CHECK(sys.find_open_buildings_in_zone(1).size() == 1);

    // Alarma: desaloja solo los edificios en el radio y los cierra hasta reabrirlos.
    for (std::uint32_t a = 10; a < 15; ++a) CHECK(sys.enter_building(a, hospital_id) == R::Success);
    const auto evacuated = sys.inject_event(EventType::Alarm, {400, 400}, 50.f);
    CHECK(evacuated.size() == 6); // 5 + agente 1
    CHECK(sys.get_occupancy(hospital_id).current_occupancy == 0);
    CHECK(sys.enter_building(20, hospital_id) == R::Closed);
    sys.reopen_building(hospital_id);
    CHECK(sys.enter_building(20, hospital_id) == R::Success);
    CHECK(sys.inject_event(EventType::Rain, {400, 400}, 50.f).empty());

    sys.update_maintenance(10.f);
    CHECK(sys.get_occupancy(hospital_id).condition < 1.f);
    CHECK(sys.stats.total_occupancy == 1);
}

// ---------------------------------------------------------------------------------------------
/// Cuadrícula n x n de calles bidireccionales separadas 100 m.
static world::NavGraph make_grid(int n) {
    world::NavGraph g;
    auto node_id = [n](int x, int y) { return static_cast<std::uint32_t>(y * n + x + 1); };
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            world::NavNode node;
            node.id = node_id(x, y);
            node.position = {x * 100.f, y * 100.f};
            g.nodes.emplace(node.id, node);
        }
    auto connect = [&](std::uint32_t a, std::uint32_t b) {
        world::NavEdge e;
        e.id = g.next_edge_id++;
        e.from_node = a;
        e.to_node = b;
        e.max_speed = 13.9f;
        e.length = 100.f;
        g.edges.emplace(e.id, e);
        g.nodes.at(a).edges.push_back(e.id);
        g.nodes.at(b).edges.push_back(e.id);
    };
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            if (x + 1 < n) connect(node_id(x, y), node_id(x + 1, y));
            if (y + 1 < n) connect(node_id(x, y), node_id(x, y + 1));
        }
    return g;
}

static void test_traffic() {
    std::puts("traffic");
    const auto graph = make_grid(5);
    simulation::systems::TrafficSystemConfig cfg;
    simulation::systems::TrafficSystem traffic(cfg, graph);
    traffic.initialize(500, 1234);

    CHECK(traffic.vehicles().size() == 100);
    CHECK(traffic.get_all_light_states().size() == 9); // cruces interiores de 4 tramos

    // Ruta de esquina a esquina: 8 tramos de 100 m.
    const auto route = traffic.find_route(1, 25);
    CHECK(route.size() == 8);

    bool any_waited = false, any_moving = false;
    for (int t = 0; t < 900; ++t) {
        traffic.update(1.f, 12.f, 2);
        any_waited |= traffic.stats.vehicles_waiting > 0;
        any_moving |= traffic.stats.vehicles_moving > 0;
    }
    CHECK(any_moving);
    CHECK(any_waited);                      // los semáforos detienen vehículos
    CHECK(traffic.stats.trips_completed > 50);
    CHECK(traffic.stats.avg_speed > 0.f && traffic.stats.avg_speed <= 13.9f + 1e-3f);
    bool positions_ok = true;
    for (const auto& v : traffic.vehicles())
        positions_ok &= v.position.x >= -1e-3f && v.position.x <= 400.001f && v.position.y >= -1e-3f && v.position.y <= 400.001f;
    CHECK(positions_ok);

    // Hora punta reduce la velocidad (antes la aumentaba).
    const float normal = traffic.get_speed_limit(1, world::RoadType::Local);
    traffic.update(0.f, 8.f, 1);
    CHECK(traffic.get_speed_limit(1, world::RoadType::Local) < normal);
    traffic.update(0.f, 8.f, 6); // domingo: sin hora punta
    CHECK(std::abs(traffic.get_speed_limit(1, world::RoadType::Local) - normal) < 1e-4f);

    // Corte de tráfico en el centro: la ruta lo evita, y si se corta todo no hay ruta.
    traffic.inject_traffic_block({200, 200}, 60.f, 100.f);
    const auto detour = traffic.find_route(11, 15); // fila central, de borde a borde
    CHECK(!detour.empty());
    bool avoids = true;
    for (auto e : detour) avoids &= !traffic.is_edge_blocked(e);
    CHECK(avoids);
    CHECK(detour.size() > 4);
    traffic.inject_traffic_block({200, 200}, 1000.f, 100.f);
    CHECK(traffic.find_route(1, 25).empty());
    for (int t = 0; t < 120; ++t) traffic.update(1.f, 12.f, 2);
    CHECK(traffic.stats.vehicles_blocked + traffic.stats.vehicles_waiting > 0);
    for (int t = 0; t < 120; ++t) traffic.update(1.f, 12.f, 2); // el corte expira a los 100 s
    CHECK(!traffic.find_route(1, 25).empty());

    // Determinismo: misma semilla, mismo resultado.
    simulation::systems::TrafficSystem a(cfg, graph), b(cfg, graph);
    a.initialize(300, 99);
    b.initialize(300, 99);
    for (int t = 0; t < 300; ++t) { a.update(1.f, 9.f, 1); b.update(1.f, 9.f, 1); }
    bool same = a.vehicles().size() == b.vehicles().size();
    for (std::size_t i = 0; same && i < a.vehicles().size(); ++i)
        same = a.vehicles()[i].position.x == b.vehicles()[i].position.x && a.vehicles()[i].position.y == b.vehicles()[i].position.y;
    CHECK(same);
}

int main() {
    test_personality();
    test_opening_hours();
    test_buildings();
    test_traffic();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
