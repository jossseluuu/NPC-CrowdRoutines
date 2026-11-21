#include "traffic_system.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace npc_crowd::simulation::systems {

namespace {
constexpr float kDefaultLightCycle = 30.f + 3.f + 25.f + 3.f + 2.f;

TrafficLightState::Phase phase_at(const TrafficLightConfig& cfg, float t, float& time_until_change) {
    using P = TrafficLightState::Phase;
    float edge = cfg.ns_green;
    if (t < edge) { time_until_change = edge - t; return P::NS_Green; }
    edge += cfg.ns_yellow;
    if (t < edge) { time_until_change = edge - t; return P::NS_Yellow; }
    edge += cfg.ew_green;
    if (t < edge) { time_until_change = edge - t; return P::EW_Green; }
    edge += cfg.ew_yellow;
    if (t < edge) { time_until_change = edge - t; return P::EW_Yellow; }
    time_until_change = cfg.cycle() - t;
    return P::All_Red;
}
} // namespace

TrafficSystem::TrafficSystem(const TrafficSystemConfig& config, const world::NavGraph& graph)
    : config_(config), graph_(graph) {}

Vec2 TrafficSystem::node_pos(std::uint32_t node_id) const {
    const auto it = graph_.nodes.find(node_id);
    return it != graph_.nodes.end() ? it->second.position : Vec2{};
}

std::uint32_t TrafficSystem::other_end(const world::NavEdge& edge, std::uint32_t node) const {
    return edge.from_node == node ? edge.to_node : edge.from_node;
}

float TrafficSystem::edge_length(const world::NavEdge& edge) const {
    if (edge.length > 0.f) return edge.length;
    return std::max(0.1f, std::sqrt(length_sq(node_pos(edge.to_node) - node_pos(edge.from_node))));
}

float TrafficSystem::rush_hour_divisor() const {
    const float h = current_world_hour_;
    const bool weekday = current_week_day_ >= 0 && current_week_day_ <= 4;
    const bool rush = weekday && ((h >= 7.f && h <= 10.f) || (h >= 16.f && h <= 19.f));
    return rush ? std::max(1.f, config_.rush_hour_factor) : 1.f;
}

// ---------------------------------------------------------------------------------------------
// Inicialización
// ---------------------------------------------------------------------------------------------
void TrafficSystem::initialize(std::uint32_t population, std::uint64_t seed) {
    rng_.seed(seed);
    system_vehicles_.clear();
    blocked_edges_.clear();
    stats = Stats{};

    node_list_.clear();
    for (const auto& [id, node] : graph_.nodes)
        if (!node.edges.empty()) node_list_.push_back(id);
    std::sort(node_list_.begin(), node_list_.end());

    initialize_lights();
    if (node_list_.size() < 2) return;

    const auto count = static_cast<std::uint32_t>(static_cast<float>(population) * config_.vehicles_per_resident);
    std::uniform_int_distribution<std::size_t> pick(0, node_list_.size() - 1);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    system_vehicles_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        SystemVehicle v;
        v.id = i + 1;
        const float r = unit(rng_);
        v.type = r < .02f ? SystemVehicle::Type::Emergency : r < .08f ? SystemVehicle::Type::Bus
               : r < .15f ? SystemVehicle::Type::Delivery : r < .22f ? SystemVehicle::Type::Taxi
               : r < .30f ? SystemVehicle::Type::Bike : SystemVehicle::Type::Car;
        v.target_speed = v.type == SystemVehicle::Type::Bike ? 5.f : v.type == SystemVehicle::Type::Bus ? 11.f : 13.9f;
        const auto start = node_list_[pick(rng_)];
        v.position = node_pos(start);
        assign_new_trip(v, start);
        system_vehicles_.push_back(std::move(v));
    }
    stats.total_vehicles = system_vehicles_.size();
}

void TrafficSystem::initialize_lights() {
    lights_.clear();
    light_configs_.clear();
    node_light_.clear();

    // 1) Semáforos definidos en el grafo.
    for (const auto& [id, tl] : graph_.traffic_lights) {
        TrafficLightConfig cfg;
        cfg.node_id = tl.node_id;
        cfg.ns_green = tl.ns_green_duration;
        cfg.ns_yellow = tl.ns_yellow_duration;
        cfg.ew_green = tl.ew_green_duration;
        cfg.ew_yellow = tl.ew_yellow_duration;
        cfg.all_red = tl.all_red_duration;
        cfg.pedestrian_cross_time = tl.pedestrian_cross_time;
        cfg.controlled_edges = tl.controlled_edges;
        light_configs_[id] = cfg;
    }
    // 2) Si el grafo no trae semáforos, se colocan en los cruces de 4 o más tramos (no en calles peatonales).
    if (light_configs_.empty()) {
        std::uint32_t next_id = 1;
        for (auto node_id : node_list_) {
            const auto& node = graph_.nodes.at(node_id);
            if (node.edges.size() < 4) continue;
            TrafficLightConfig cfg;
            cfg.node_id = node_id;
            cfg.controlled_edges = node.edges;
            light_configs_[next_id++] = cfg;
        }
    }
    // Desfase por nodo para que no todos los semáforos cambien a la vez (onda verde aproximada).
    for (const auto& [id, cfg] : light_configs_) {
        TrafficLightState s;
        s.light_id = id;
        s.node_id = cfg.node_id;
        s.cycle_length = cfg.cycle() > 0.f ? cfg.cycle() : kDefaultLightCycle;
        s.phase_timer = std::fmod(static_cast<float>(cfg.node_id) * 7.3f, s.cycle_length);
        s.pedestrian_cross_time = cfg.pedestrian_cross_time;
        s.current_phase = phase_at(cfg, s.phase_timer, s.time_until_change);
        lights_[id] = s;
        node_light_[cfg.node_id] = id;
    }
}

// ---------------------------------------------------------------------------------------------
// Rutas
// ---------------------------------------------------------------------------------------------
std::vector<std::uint32_t> TrafficSystem::find_route(std::uint32_t from_node, std::uint32_t to_node) const {
    if (from_node == to_node || !graph_.nodes.count(from_node) || !graph_.nodes.count(to_node)) return {};
    using Item = std::pair<float, std::uint32_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    std::unordered_map<std::uint32_t, float> dist;
    std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> came_from; // node -> (prev node, edge)
    dist[from_node] = 0.f;
    open.emplace(0.f, from_node);

    while (!open.empty()) {
        const auto [d, u] = open.top();
        open.pop();
        if (u == to_node) break;
        if (d > dist[u]) continue;
        for (auto edge_id : graph_.nodes.at(u).edges) {
            const auto eit = graph_.edges.find(edge_id);
            if (eit == graph_.edges.end() || blocked_edges_.count(edge_id)) continue;
            const auto& e = eit->second;
            if (e.road_type == world::RoadType::Pedestrian) continue;      // sin vehículos
            if (!e.is_bidirectional && e.from_node != u) continue;         // sentido único
            const auto v = other_end(e, u);
            const float speed = e.max_speed > 0.f ? e.max_speed : config_.default_speed_limit;
            const float nd = d + edge_length(e) / speed;                   // coste = tiempo
            const auto old = dist.find(v);
            if (old == dist.end() || nd < old->second) {
                dist[v] = nd;
                came_from[v] = {u, edge_id};
                open.emplace(nd, v);
            }
        }
    }
    if (!came_from.count(to_node)) return {};
    std::vector<std::uint32_t> path;
    for (auto n = to_node; n != from_node; n = came_from.at(n).first) path.push_back(came_from.at(n).second);
    std::reverse(path.begin(), path.end());
    return path;
}

bool TrafficSystem::assign_new_trip(SystemVehicle& veh, std::uint32_t from_node) {
    if (node_list_.size() < 2) return false;
    std::uniform_int_distribution<std::size_t> pick(0, node_list_.size() - 1);
    for (int attempt = 0; attempt < 8; ++attempt) {
        const auto goal = node_list_[pick(rng_)];
        auto path = find_route(from_node, goal);
        if (path.empty()) continue;
        veh.destination_node = goal;
        veh.destination = node_pos(goal);
        veh.path_edges = std::move(path);
        veh.edge_from_node = from_node;
        veh.current_edge = veh.path_edges.front();
        veh.progress_on_edge = 0.f;
        return true;
    }
    veh.path_edges.clear();
    veh.current_edge = 0;
    veh.edge_from_node = from_node;
    return false;
}

/// Pasa al siguiente edge de la ruta. Si está cortado, recalcula; devuelve false si no hay salida.
bool TrafficSystem::start_next_edge(SystemVehicle& veh) {
    const auto& finished = graph_.edges.at(veh.current_edge);
    const auto node = other_end(finished, veh.edge_from_node);
    veh.path_edges.erase(veh.path_edges.begin());
    veh.edge_from_node = node;
    veh.progress_on_edge = 0.f;

    if (veh.path_edges.empty()) { // destino alcanzado
        ++veh.trips_completed;
        ++stats.trips_completed;
        veh.current_edge = 0;
        return assign_new_trip(veh, node);
    }
    if (blocked_edges_.count(veh.path_edges.front())) {
        auto detour = find_route(node, veh.destination_node);
        if (detour.empty()) {
            veh.path_edges.insert(veh.path_edges.begin(), veh.current_edge); // se queda esperando al final del tramo
            veh.edge_from_node = other_end(finished, node);
            veh.progress_on_edge = 1.f;
            return false;
        }
        veh.path_edges = std::move(detour);
    }
    veh.current_edge = veh.path_edges.front();
    return true;
}

// ---------------------------------------------------------------------------------------------
// Actualización
// ---------------------------------------------------------------------------------------------
void TrafficSystem::update(float dt, float world_hour, int week_day,
                           const std::unordered_map<std::uint32_t, float>& pedestrian_density_by_edge) {
    current_world_hour_ = world_hour;
    current_week_day_ = week_day;

    for (auto it = blocked_edges_.begin(); it != blocked_edges_.end();) {
        it->second -= dt;
        it = it->second <= 0.f ? blocked_edges_.erase(it) : std::next(it);
    }
    if (config_.enable_traffic_lights) update_lights(dt);

    const auto trips = stats.trips_completed;
    const auto wait = stats.total_wait_time;
    stats = Stats{};
    stats.trips_completed = trips;
    stats.total_wait_time = wait;

    const float speed_factor = 1.f / rush_hour_divisor();
    float speed_sum = 0.f;
    for (auto& veh : system_vehicles_) {
        if (!veh.is_active) continue;
        ++stats.total_vehicles;
        veh.last_result = update_vehicle(veh, dt, speed_factor, pedestrian_density_by_edge);
        switch (veh.last_result) {
            case VehicleMovementResult::WaitingAtLight:
                ++stats.vehicles_waiting;
                ++stats.vehicles_stopped;
                stats.total_wait_time += dt;
                break;
            case VehicleMovementResult::Blocked:
            case VehicleMovementResult::OffRoute:
                ++stats.vehicles_blocked;
                ++stats.vehicles_stopped;
                break;
            default: {
                const float speed = std::sqrt(length_sq(veh.velocity));
                if (speed > 0.f) { ++stats.vehicles_moving; speed_sum += speed; }
                else ++stats.vehicles_stopped;
            }
        }
    }
    stats.avg_speed = stats.vehicles_moving ? speed_sum / static_cast<float>(stats.vehicles_moving) : 0.f;
}

void TrafficSystem::update_lights(float dt) {
    for (auto& [light_id, light] : lights_) {
        const auto cfg_it = light_configs_.find(light_id);
        const TrafficLightConfig cfg = cfg_it != light_configs_.end() ? cfg_it->second : TrafficLightConfig{};
        light.cycle_length = cfg.cycle() > 0.f ? cfg.cycle() : kDefaultLightCycle;
        light.phase_timer = std::fmod(light.phase_timer + dt * config_.light_cycle_speed, light.cycle_length);
        light.current_phase = phase_at(cfg, light.phase_timer, light.time_until_change);
        light.pedestrian_request = false;
    }
}

bool TrafficSystem::can_pass_light(const SystemVehicle& veh, std::uint32_t node_id) const {
    if (!config_.enable_traffic_lights) return true;
    const auto lid = node_light_.find(node_id);
    if (lid == node_light_.end()) return true;
    if (veh.type == SystemVehicle::Type::Emergency && config_.emergency_vehicle_priority > 1.f) return true;

    // Dirección de llegada: mayoritariamente vertical = N-S, horizontal = E-W.
    const auto& edge = graph_.edges.at(veh.current_edge);
    const Vec2 d = node_pos(node_id) - node_pos(other_end(edge, node_id));
    const bool north_south = std::abs(d.y) >= std::abs(d.x);
    const auto phase = lights_.at(lid->second).current_phase;
    using P = TrafficLightState::Phase;
    return north_south ? phase == P::NS_Green : phase == P::EW_Green;
}

VehicleMovementResult TrafficSystem::update_vehicle(SystemVehicle& veh, float dt, float speed_factor,
                                                    const std::unordered_map<std::uint32_t, float>& pedestrian_density) {
    if (veh.current_edge == 0 && !assign_new_trip(veh, veh.edge_from_node)) {
        veh.velocity = {};
        return VehicleMovementResult::OffRoute;
    }

    float remaining = dt;
    for (int hops = 0; hops < 8 && remaining > 0.f; ++hops) {
        const auto& edge = graph_.edges.at(veh.current_edge);
        const float length = edge_length(edge);
        const auto to_node = other_end(edge, veh.edge_from_node);

        float speed = std::min(veh.target_speed, edge.max_speed > 0.f ? edge.max_speed : config_.default_speed_limit);
        if (veh.type == SystemVehicle::Type::Bus) speed *= std::min(1.f, config_.bus_priority);
        if (const auto ped = pedestrian_density.find(veh.current_edge); ped != pedestrian_density.end() && ped->second > 0.f) {
            speed *= std::max(0.2f, 1.f - ped->second * config_.speed_penalty_per_pedestrian);
            ++stats.pedestrian_slowdowns;
        }
        speed *= speed_factor;

        if (veh.progress_on_edge >= 1.f) {
            // Al final del tramo: semáforo y posibles cortes.
            if (!can_pass_light(veh, to_node)) {
                veh.light_timer += remaining;
                if (veh.light_timer < config_.stuck_light_timeout) {
                    if (!veh.waiting_at_light) ++stats.traffic_light_stops;
                    veh.waiting_at_light = true;
                    veh.current_light = node_light_.at(to_node);
                    veh.velocity = {};
                    return VehicleMovementResult::WaitingAtLight;
                }
            }
            veh.waiting_at_light = false;
            veh.current_light = 0;
            veh.light_timer = 0.f;
            if (!start_next_edge(veh)) {
                veh.velocity = {};
                return veh.current_edge == 0 ? VehicleMovementResult::OffRoute : VehicleMovementResult::Blocked;
            }
            continue;
        }

        const float travel = speed * remaining;
        const float left = (1.f - veh.progress_on_edge) * length;
        const Vec2 from = node_pos(veh.edge_from_node), to = node_pos(to_node);
        if (travel < left) {
            veh.progress_on_edge += travel / length;
            remaining = 0.f;
        } else {
            veh.progress_on_edge = 1.f;
            remaining -= left / std::max(speed, 0.01f);
        }
        veh.position = from + (to - from) * veh.progress_on_edge;
        veh.velocity = (to - from) * (speed / length);
    }
    return VehicleMovementResult::Success;
}

// ---------------------------------------------------------------------------------------------
// Consultas
// ---------------------------------------------------------------------------------------------
float TrafficSystem::get_speed_limit(std::uint32_t edge_id, world::RoadType road_type, float pedestrian_density) const {
    float base = config_.default_speed_limit;
    if (const auto it = graph_.edges.find(edge_id); it != graph_.edges.end() && it->second.max_speed > 0.f) {
        base = it->second.max_speed;
    } else {
        switch (road_type) {
            case world::RoadType::Highway: base = 33.3f; break;   // 120 km/h
            case world::RoadType::Arterial: base = 16.7f; break;  // 60 km/h
            case world::RoadType::Collector: base = 13.9f; break; // 50 km/h
            case world::RoadType::Local: base = 8.3f; break;      // 30 km/h
            case world::RoadType::Pedestrian: base = 2.8f; break; // 10 km/h (servicio)
        }
    }
    const float ped_factor = std::max(0.2f, 1.f - pedestrian_density * config_.speed_penalty_per_pedestrian);
    return std::max(1.f, base * ped_factor / rush_hour_divisor());
}

TrafficLightState TrafficSystem::get_light_state(std::uint32_t light_id) const {
    const auto it = lights_.find(light_id);
    return it != lights_.end() ? it->second : TrafficLightState{};
}

std::vector<TrafficLightState> TrafficSystem::get_all_light_states() const {
    std::vector<TrafficLightState> result;
    result.reserve(lights_.size());
    for (const auto& [id, light] : lights_) result.push_back(light);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.light_id < b.light_id; });
    return result;
}

std::vector<std::uint32_t> TrafficSystem::get_congested_edges(float threshold_ratio) const {
    std::unordered_map<std::uint32_t, std::pair<int, int>> per_edge; // edge -> (detenidos, total)
    for (const auto& veh : system_vehicles_) {
        if (!veh.is_active || veh.current_edge == 0) continue;
        auto& [stopped, total] = per_edge[veh.current_edge];
        ++total;
        if (veh.velocity.x == 0.f && veh.velocity.y == 0.f) ++stopped;
    }
    std::vector<std::uint32_t> result;
    for (const auto& [edge_id, counts] : per_edge)
        if (counts.second >= 2 && static_cast<float>(counts.first) / static_cast<float>(counts.second) >= threshold_ratio)
            result.push_back(edge_id);
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::uint32_t> TrafficSystem::get_gridlocked_nodes() const {
    std::unordered_map<std::uint32_t, int> waiting_at_node;
    for (const auto& veh : system_vehicles_) {
        if (!veh.is_active || veh.current_edge == 0 || veh.progress_on_edge < 1.f) continue;
        if (veh.last_result == VehicleMovementResult::Success) continue;
        ++waiting_at_node[other_end(graph_.edges.at(veh.current_edge), veh.edge_from_node)];
    }
    std::vector<std::uint32_t> result;
    for (const auto& [node_id, count] : waiting_at_node)
        if (count > 3) result.push_back(node_id);
    std::sort(result.begin(), result.end());
    return result;
}

bool TrafficSystem::is_gridlock(std::uint32_t edge_id) const {
    int stopped = 0, total = 0;
    for (const auto& veh : system_vehicles_) {
        if (!veh.is_active || veh.current_edge != edge_id) continue;
        ++total;
        if (veh.velocity.x == 0.f && veh.velocity.y == 0.f) ++stopped;
    }
    return total >= 3 && stopped * 2 > total;
}

void TrafficSystem::inject_traffic_block(Vec2 position, float radius, float duration_seconds) {
    const float r2 = radius * radius;
    for (const auto& [edge_id, edge] : graph_.edges) {
        const Vec2 mid = (node_pos(edge.from_node) + node_pos(edge.to_node)) * 0.5f;
        if (length_sq(mid - position) <= r2) blocked_edges_[edge_id] = std::max(blocked_edges_[edge_id], duration_seconds);
    }
}

void TrafficSystem::inject_event(EventType type, Vec2 position, float radius) {
    switch (type) {
        case EventType::TrafficBlock: inject_traffic_block(position, radius, 600.f); break;
        case EventType::Alarm: inject_traffic_block(position, radius, 1800.f); break; // zona acordonada
        case EventType::Rain: break; // la lluvia se modela como factor de velocidad global en la simulación
    }
}

void TrafficSystem::set_light_config(std::uint32_t light_id, const TrafficLightConfig& config) {
    light_configs_[light_id] = config;
    auto& state = lights_[light_id];
    state.light_id = light_id;
    state.node_id = config.node_id;
    state.cycle_length = config.cycle() > 0.f ? config.cycle() : kDefaultLightCycle;
    state.phase_timer = std::fmod(state.phase_timer, state.cycle_length);
    state.current_phase = phase_at(config, state.phase_timer, state.time_until_change);
    node_light_[config.node_id] = light_id;
}

} // namespace npc_crowd::simulation::systems
