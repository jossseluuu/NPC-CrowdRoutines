#include "behavior/movement.hpp"

#include "behavior/behavior.hpp"

#include <algorithm>
#include <cmath>

namespace npc::movement {

using pop::kNone;
using pop::LegKind;

namespace {

constexpr float kBusSpeed = 8.f;        // m/s cruising
constexpr float kBusDwell = 18.f;       // s per stop
constexpr int kBusCapacity = 60;
constexpr float kMaxBusWait = 25.f * 60.f;

Vec2 node_pos(const sim::SimState& s, std::uint32_t n) { return s.map.nodes[n].pos; }

float lane_capacity(const world::Edge& e) {
    const float lanes = e.kind == world::RoadKind::Avenue ? 2.f : 1.f;
    return std::max(2.f, e.length / 7.5f * lanes);
}

/// Distance along a bus line at path index k.
float line_at(const sim::BusLineGeom& g, std::size_t k) { return g.cumulative[k]; }

Vec2 line_point(const sim::SimState& s, std::uint32_t line, float d) {
    const auto& g = s.lines[line];
    const auto& path = s.map.bus_lines[line].path;
    d = std::clamp(d, 0.f, g.length);
    const auto it = std::upper_bound(g.cumulative.begin(), g.cumulative.end(), d);
    const std::size_t k = it == g.cumulative.begin() ? 0 : static_cast<std::size_t>(it - g.cumulative.begin()) - 1;
    if (k + 1 >= path.size()) return node_pos(s, path.back());
    const float seg = g.cumulative[k + 1] - g.cumulative[k];
    const float t = seg > 0 ? (d - g.cumulative[k]) / seg : 0.f;
    return lerp(node_pos(s, path[k]), node_pos(s, path[k + 1]), t);
}

int travel_substate(const sim::SimState& s, std::uint32_t i) {
    const auto& a = s.agents;
    const auto& cat = s.cat;
    const auto& mv = a.move[i];
    const auto& leg = mv.legs[mv.leg];
    switch (leg.kind) {
        case LegKind::Bike: return cat.sub("cycling");
        case LegKind::Wait: return cat.sub("waiting_bus");
        case LegKind::Ride: return cat.sub("on_bus");
        case LegKind::Drive: {
            const std::uint32_t e = mv.edge_index < leg.route.size() ? (*leg.route.edges)[mv.edge_index] : kNone;
            if (e != kNone && s.cars_on_edge[e] > lane_capacity(s.map.edges[e]) * 0.9f) return cat.sub("stuck_in_traffic");
            return cat.sub("driving");
        }
        case LegKind::Walk: break;
    }
    if (mv.wait > 0.5f && cat.sub("waiting_crossing") >= 0) return cat.sub("waiting_crossing");
    if (a.group[i] != kNone) return cat.sub("walking_talking");
    const int pending = a.pending_activity[i];
    // Late for an obligation -> hurrying; going out for fun -> strolling.
    for (const auto& it : a.agenda[i])
        if (it.hard && it.activity == pending && s.hour() > it.start - 0.02 && s.hour() < it.end) return cat.sub("hurrying");
    if (pending == cat.activity_index("leisure") || pending == cat.act_social) return cat.sub("strolling");
    return cat.sub("walking");
}

float walk_speed(const sim::SimState& s, std::uint32_t i) {
    const auto& a = s.agents;
    const auto& cat = s.cat;
    const int sub = a.substate[i];
    float v = 1.3f * (0.85f + 0.3f * a.traits[i][data::ActivityLevel]);
    if (a.activity[i] == cat.act_emergency) {
        if (sub == cat.sub("fleeing")) v = 2.6f;
        else if (sub == cat.sub("pursuing")) v = 2.9f;
        else if (sub == cat.sub("evacuating")) v = 1.8f;
        else v = 1.45f;
    } else if (a.group[i] != kNone) v = 1.15f; // groups walk at a common pace (keeps them together)
    else if (sub == cat.sub("hurrying")) v = 1.85f;
    else if (sub == cat.sub("strolling")) v = 0.95f;
    if (a.group[i] == kNone) {
        if (a.age[i] >= 72) v *= 0.78f;
        else if (a.age[i] < 10) v *= 0.9f;
    }
    return v;
}

void leave_building(sim::SimState& s, std::uint32_t i) {
    auto& a = s.agents;
    if (a.location[i] != kNone) {
        auto& occ = s.occupancy[a.location[i]];
        if (occ > 0) --occ;
        a.location[i] = kNone;
    }
    a.outdoor[i] = 1;
}

void set_travel_substate(sim::SimState& s, std::uint32_t i) {
    auto& a = s.agents;
    if (a.activity[i] != s.cat.act_travelling) return;
    a.substate[i] = static_cast<std::uint8_t>(travel_substate(s, i)); // no history entry: changes too often
}

bool plan_bus(sim::SimState& s, std::uint32_t origin, std::uint32_t dest, pop::MoveState& mv) {
    const auto& map = s.map;
    const Vec2 o = node_pos(s, origin), d = node_pos(s, dest);
    const float walk_only = distance(o, d) * 1.3f / 1.35f;
    float best_cost = walk_only;
    std::uint32_t best_line = kNone, best_a = 0, best_b = 0;
    for (std::uint32_t l = 0; l < map.bus_lines.size(); ++l) {
        const auto& line = map.bus_lines[l];
        const auto& g = s.lines[l];
        std::uint32_t ka = 0, kb = 0;
        float da = 1e30f, db = 1e30f;
        for (std::uint32_t k = 0; k < line.path.size(); ++k) {
            if (!g.is_stop[k]) continue;
            const float x = distance(node_pos(s, line.path[k]), o), y = distance(node_pos(s, line.path[k]), d);
            if (x < da) { da = x; ka = k; }
            if (y < db) { db = y; kb = k; }
        }
        if (ka == kb) continue;
        const float ride = std::abs(line_at(g, kb) - line_at(g, ka));
        if (ride < 450.f || da + db > 1000.f) continue;
        const float cost = (da + db) * 1.3f / 1.35f + 240.f + ride / 7.f;
        if (cost < best_cost) { best_cost = cost; best_line = l; best_a = ka; best_b = kb; }
    }
    if (best_line == kNone) return false;
    const auto& line = map.bus_lines[best_line];
    world::Route r1, r2;
    if (!s.router->route(origin, line.path[best_a], world::TravelMode::Walk, r1)) return false;
    if (!s.router->route(line.path[best_b], dest, world::TravelMode::Walk, r2)) return false;
    mv.legs[0] = {LegKind::Walk, r1, kNone, 0, 0};
    mv.legs[1] = {LegKind::Wait, {}, best_line, best_a, best_b};
    mv.legs[1].route.start = line.path[best_a];
    mv.legs[2] = {LegKind::Ride, {}, best_line, best_a, best_b};
    mv.legs[2].route.start = line.path[best_a];
    mv.legs[3] = {LegKind::Walk, r2, kNone, 0, 0};
    mv.leg_count = 4;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------------------------
void init_transport(sim::SimState& s) {
    const auto& map = s.map;
    s.lines.clear();
    s.buses.clear();
    for (std::uint32_t l = 0; l < map.bus_lines.size(); ++l) {
        const auto& line = map.bus_lines[l];
        sim::BusLineGeom g;
        g.cumulative.assign(line.path.size(), 0.f);
        g.is_stop.assign(line.path.size(), 0);
        for (std::size_t k = 1; k < line.path.size(); ++k)
            g.cumulative[k] = g.cumulative[k - 1] + distance(node_pos(s, line.path[k - 1]), node_pos(s, line.path[k]));
        g.length = g.cumulative.empty() ? 0.f : g.cumulative.back();
        for (auto stop : line.stops)
            for (std::size_t k = 0; k < line.path.size(); ++k)
                if (line.path[k] == stop) g.is_stop[k] = 1;
        s.lines.push_back(std::move(g));
        const int count = std::max(2, static_cast<int>(s.lines.back().length / 520.f));
        for (int b = 0; b < count; ++b) {
            sim::Bus bus;
            bus.line = l;
            const float cyc = 2.f * s.lines.back().length * static_cast<float>(b) / static_cast<float>(count);
            bus.dir = cyc < s.lines.back().length ? 1 : -1;
            bus.s = bus.dir > 0 ? cyc : 2.f * s.lines.back().length - cyc;
            bus.pos = line_point(s, l, bus.s);
            s.buses.push_back(bus);
        }
    }
    s.cars_on_edge.assign(map.edges.size(), 0);
    s.cars_next.assign(map.edges.size(), 0);
    s.edge_speed_factor.assign(map.edges.size(), 1.f);
    s.walk_factor_edge.assign(map.edges.size(), 1.f);
}

bool start_trip(sim::SimState& s, std::uint32_t i, std::uint32_t target_building, Vec2 target_point, world::TravelMode mode) {
    auto& a = s.agents;
    auto& mv = a.move[i];
    const auto& map = s.map;
    const std::uint32_t origin = a.location[i] != kNone ? map.buildings[a.location[i]].entrance_node : map.nearest_node(a.pos[i]);
    const std::uint32_t dest = target_building != kNone ? map.buildings[target_building].entrance_node : map.nearest_node(target_point);
    pop::MoveState plan;
    plan.target_building = target_building;
    plan.target_point = target_point;
    bool ok = false;
    if (mode == world::TravelMode::Bus) ok = plan_bus(s, origin, dest, plan);
    if (!ok && mode == world::TravelMode::Car) {
        const auto oc = map.nearest_node_with_cars(node_pos(s, origin)), dc = map.nearest_node_with_cars(node_pos(s, dest));
        world::Route r;
        if (oc != dc && s.router->route(oc, dc, world::TravelMode::Car, r)) {
            plan.legs[0] = {LegKind::Drive, r, kNone, 0, 0};
            plan.leg_count = 1;
            ok = true;
        }
    }
    if (!ok) {
        const auto m = mode == world::TravelMode::Bike ? world::TravelMode::Bike : world::TravelMode::Walk;
        world::Route r;
        if (!s.router->route(origin, dest, m, r)) return false;
        plan.legs[0] = {m == world::TravelMode::Bike ? LegKind::Bike : LegKind::Walk, r, kNone, 0, 0};
        plan.leg_count = 1;
    }
    plan.leg = 0;
    plan.edge_index = 0;
    plan.at_node = plan.legs[0].route.start;
    plan.t = 0;
    plan.active = true;
    leave_building(s, i);
    a.pos[i] = node_pos(s, plan.at_node);
    mv = std::move(plan);
    if (a.activity[i] != s.cat.act_emergency) {
        behavior::set_state(s, i, s.cat.act_travelling, s.cat.sub("walking"));
        set_travel_substate(s, i);
    }
    return true;
}

void join_trip(sim::SimState& s, std::uint32_t follower, std::uint32_t leader) {
    auto& a = s.agents;
    leave_building(s, follower);
    a.move[follower] = a.move[leader];
    a.group[follower] = leader;
    a.group[leader] = leader;
    a.pos[follower] = a.pos[leader];
    behavior::set_state(s, follower, s.cat.act_travelling, s.cat.sub("walking_talking"));
    set_travel_substate(s, leader);
}

void cancel_trip(sim::SimState& s, std::uint32_t i) {
    auto& a = s.agents;
    auto& mv = a.move[i];
    if (mv.active && mv.legs[mv.leg].kind == LegKind::Ride && mv.bus != kNone && s.buses[mv.bus].passengers > 0) --s.buses[mv.bus].passengers;
    mv.active = false;
    mv.bus = kNone;
    a.group[i] = kNone;
    a.speed[i] = 0;
    leave_building(s, i);
}

float walk_factor(const sim::SimState& s, std::uint32_t i) {
    const auto& mv = s.agents.move[i];
    float f = s.weather.walk;
    if (mv.active) {
        const auto& leg = mv.legs[mv.leg];
        if (mv.edge_index < leg.route.size()) f *= s.walk_factor_edge[(*leg.route.edges)[mv.edge_index]];
    }
    return f;
}

// ---------------------------------------------------------------------------------------------
namespace {

void update_buses(sim::SimState& s, float dt) {
    for (auto& bus : s.buses) {
        const auto& g = s.lines[bus.line];
        if (g.length <= 0) continue;
        if (bus.delay > 0) { bus.delay -= dt; continue; }
        if (bus.dwell > 0) {
            bus.dwell -= dt;
            if (bus.dwell <= 0) bus.at_stop = kNone;
            continue;
        }
        const float old = bus.s;
        bus.s += static_cast<float>(bus.dir) * kBusSpeed * s.weather.car * dt;
        // Stop at the first stop passed in this step.
        const auto& path_stops = g.is_stop;
        const float lo = std::min(old, bus.s), hi = std::max(old, bus.s);
        std::uint32_t hit = kNone;
        for (std::uint32_t k = 0; k < path_stops.size(); ++k) {
            if (!path_stops[k]) continue;
            const float d = g.cumulative[k];
            if (d > lo && d <= hi) {
                if (hit == kNone || (bus.dir > 0 ? d < g.cumulative[hit] : d > g.cumulative[hit])) hit = k;
            }
        }
        if (bus.s >= g.length) { bus.s = g.length; bus.dir = -1; hit = static_cast<std::uint32_t>(g.cumulative.size() - 1); }
        else if (bus.s <= 0) { bus.s = 0; bus.dir = 1; hit = 0; }
        if (hit != kNone) {
            bus.s = g.cumulative[hit];
            bus.at_stop = hit;
            bus.dwell = kBusDwell;
        }
        bus.pos = line_point(s, bus.line, bus.s);
    }
}

/// Moves along the current leg's route. Returns true when the leg is complete.
bool advance_route(sim::SimState& s, std::uint32_t i, float speed_base, float dt, bool car, bool bike) {
    auto& a = s.agents;
    auto& mv = a.move[i];
    auto& leg = mv.legs[mv.leg];
    const auto& map = s.map;
    float remaining = dt;
    float travelled = 0;
    while (remaining > 1e-4f && mv.edge_index < leg.route.size()) {
        const std::uint32_t e = (*leg.route.edges)[mv.edge_index];
        const auto& edge = map.edges[e];
        const std::uint32_t from = mv.at_node, to = edge.other(from);
        float speed = speed_base;
        if (car) {
            const float load = static_cast<float>(s.cars_on_edge[e]) / lane_capacity(edge);
            speed = edge.car_speed() * s.edge_speed_factor[e] * s.weather.car / (1.f + load * load);
            speed = std::max(speed, 0.6f);
            if (s.cars_next[e] < 65535) ++s.cars_next[e];
        } else if (!bike) {
            speed *= s.walk_factor_edge[e];
        }
        if (mv.t >= 1.f) {
            // At the end of the edge: traffic light before entering the next one.
            const bool has_next = mv.edge_index + 1 < leg.route.size();
            if (has_next && map.nodes[to].traffic_light) {
                const std::uint32_t ne = (*leg.route.edges)[mv.edge_index + 1];
                const Vec2 dir = car ? node_pos(s, to) - node_pos(s, from) : node_pos(s, map.edges[ne].other(to)) - node_pos(s, to);
                const bool emergency_runner = a.activity[i] == s.cat.act_emergency;
                if (!s.green_for(to, dir) && !emergency_runner && mv.wait < (car ? 90.f : 50.f)) {
                    mv.wait += remaining;
                    a.speed[i] = 0;
                    break;
                }
            }
            mv.wait = 0;
            mv.at_node = to;
            ++mv.edge_index;
            mv.t = 0;
            continue;
        }
        const float left = (1.f - mv.t) * edge.length;
        const float step = speed * remaining;
        if (step < left) {
            mv.t += step / edge.length;
            travelled += step;
            remaining = 0;
        } else {
            travelled += left;
            remaining -= left / speed;
            mv.t = 1.f;
        }
        a.speed[i] = speed;
    }
    a.distance[i] += travelled;
    // Position: offset to the right-hand side (pavement for pedestrians, lane for vehicles).
    if (mv.edge_index < leg.route.size()) {
        const std::uint32_t e = (*leg.route.edges)[mv.edge_index];
        const Vec2 p0 = node_pos(s, mv.at_node), p1 = node_pos(s, map.edges[e].other(mv.at_node));
        const Vec2 dir = normalized(p1 - p0);
        float side = car ? 1.8f : bike ? 2.7f : (map.edges[e].kind == world::RoadKind::Path ? 1.0f : 3.6f);
        if (a.group[i] != kNone && a.group[i] != i) side += 1.1f;
        a.pos[i] = lerp(p0, p1, std::min(mv.t, 1.f)) + Vec2{-dir.y, dir.x} * side;
        return false;
    }
    a.pos[i] = node_pos(s, mv.at_node);
    return true;
}

/// Advances resident i. Returns true on arrival at the final destination.
bool step_agent(sim::SimState& s, std::uint32_t i, float dt) {
    auto& a = s.agents;
    auto& mv = a.move[i];
    for (int guard = 0; guard < 6 && mv.leg < mv.leg_count; ++guard) {
        auto& leg = mv.legs[mv.leg];
        bool done = false;
        switch (leg.kind) {
            case LegKind::Walk:
                done = advance_route(s, i, walk_speed(s, i) * s.weather.walk, dt, false, false);
                break;
            case LegKind::Bike:
                done = advance_route(s, i, 4.4f * s.weather.walk, dt, false, true);
                break;
            case LegKind::Drive:
                done = advance_route(s, i, 0.f, dt, true, false);
                break;
            case LegKind::Wait: {
                const auto& line = s.map.bus_lines[leg.line];
                a.pos[i] = node_pos(s, line.path[leg.board_stop]) + Vec2{static_cast<float>(i % 5) - 2.f, 3.f + static_cast<float>(i % 3)};
                a.speed[i] = 0;
                mv.wait += dt;
                const int needed_dir = leg.alight_stop > leg.board_stop ? 1 : -1;
                for (std::uint32_t b = 0; b < s.buses.size(); ++b) {
                    auto& bus = s.buses[b];
                    if (bus.line != leg.line || bus.at_stop != leg.board_stop || bus.dir != needed_dir || bus.passengers >= kBusCapacity) continue;
                    ++bus.passengers;
                    mv.bus = b;
                    a.log(i, s.now(), pop::ActionKind::Boarded, leg.line);
                    done = true;
                    break;
                }
                if (!done && mv.wait > kMaxBusWait) {
                    // Gave up waiting: walk instead.
                    const auto target = mv.target_building;
                    const auto point = mv.target_point;
                    a.log(i, s.now(), pop::ActionKind::ChangedPlan);
                    mv.active = false;
                    const std::uint32_t here = s.map.nearest_node(a.pos[i]);
                    world::Route r;
                    const std::uint32_t dest = target != kNone ? s.map.buildings[target].entrance_node : s.map.nearest_node(point);
                    if (s.router->route(here, dest, world::TravelMode::Walk, r)) {
                        mv.legs[0] = {LegKind::Walk, r, kNone, 0, 0};
                        mv.leg_count = 1;
                        mv.leg = 0;
                        mv.edge_index = 0;
                        mv.at_node = here;
                        mv.t = 0;
                        mv.wait = 0;
                        mv.active = true;
                    }
                    return !mv.active;
                }
                if (!done) return false;
                mv.wait = 0;
                break;
            }
            case LegKind::Ride: {
                if (mv.bus == kNone) { done = true; break; }
                auto& bus = s.buses[mv.bus];
                a.pos[i] = bus.pos;
                a.speed[i] = bus.dwell > 0 ? 0.f : kBusSpeed;
                if (bus.at_stop == leg.alight_stop) {
                    if (bus.passengers > 0) --bus.passengers;
                    mv.bus = kNone;
                    done = true;
                } else {
                    return false;
                }
                break;
            }
        }
        if (!done) return false;
        ++mv.leg;
        if (mv.leg >= mv.leg_count) break;
        mv.edge_index = 0;
        mv.t = 0;
        mv.wait = 0;
        mv.at_node = mv.legs[mv.leg].route.start;
        dt = 0.f; // the remaining time of this tick is not carried into the next leg
    }
    if (mv.leg >= mv.leg_count) {
        mv.active = false;
        if (mv.target_building == kNone) a.pos[i] = mv.target_point;
        return true;
    }
    return false;
}

} // namespace

void step(sim::SimState& s, float dt) {
    auto& a = s.agents;
    std::swap(s.cars_on_edge, s.cars_next);
    std::fill(s.cars_next.begin(), s.cars_next.end(), 0);
    update_buses(s, dt);
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        if (!a.move[i].active) continue;
        if (step_agent(s, i, dt)) {
            behavior::on_arrival(s, i);
        } else {
            set_travel_substate(s, i);
        }
    }
}

} // namespace npc::movement
