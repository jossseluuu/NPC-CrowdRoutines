#include "world/navigation.hpp"

#include <algorithm>
#include <limits>
#include <queue>

namespace npc::world {

namespace {
constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t kMaxCache = 60000;
} // namespace

const char* travel_mode_name(TravelMode m) {
    switch (m) {
        case TravelMode::Walk: return "walk";
        case TravelMode::Bike: return "bike";
        case TravelMode::Bus: return "bus";
        case TravelMode::Car: return "car";
    }
    return "walk";
}

Router::Router(const CityMap& map)
    : map_(map), car_blocked_(map.edges.size(), 0), hazard_(map.edges.size(), 0.f),
      g_(map.nodes.size(), 0.f), came_edge_(map.nodes.size(), kNone), stamp_(map.nodes.size(), 0) {}

float Router::mode_speed(TravelMode mode, const Edge& e) {
    switch (mode) {
        case TravelMode::Car: return e.cars() ? e.car_speed() : 0.f;
        case TravelMode::Bike: return e.kind == RoadKind::Path ? 3.0f : 4.6f;
        case TravelMode::Walk:
        case TravelMode::Bus: return e.kind == RoadKind::Avenue ? 1.30f : 1.38f; // crossings on avenues take longer
    }
    return 1.35f;
}

float Router::edge_cost(TravelMode mode, std::uint32_t ei) const {
    const auto& e = map_.edges[ei];
    const float speed = mode_speed(mode, e);
    if (speed <= 0.f) return -1.f;
    if (mode == TravelMode::Car && car_blocked_[ei]) return -1.f;
    float cost = e.length / speed;
    if (mode != TravelMode::Car) cost += hazard_[ei];
    else if (map_.nodes[e.a].traffic_light || map_.nodes[e.b].traffic_light) cost += 8.f; // expected wait
    return cost;
}

void Router::set_car_blocked(std::uint32_t edge, bool blocked) {
    if (edge >= car_blocked_.size() || (car_blocked_[edge] != 0) == blocked) return;
    car_blocked_[edge] = blocked ? 1 : 0;
    ++version_;
}

void Router::set_hazard(std::uint32_t edge, float extra_seconds) {
    if (edge >= hazard_.size() || hazard_[edge] == extra_seconds) return;
    hazard_[edge] = extra_seconds;
    ++version_;
}

void Router::clear_closures() {
    std::fill(car_blocked_.begin(), car_blocked_.end(), 0);
    std::fill(hazard_.begin(), hazard_.end(), 0.f);
    ++version_;
}

bool Router::route(std::uint32_t from, std::uint32_t to, TravelMode mode, Route& out) {
    out.start = from;
    out.edges.reset();
    if (from >= map_.nodes.size() || to >= map_.nodes.size()) return false;
    if (from == to) {
        static const auto empty = std::make_shared<const std::vector<std::uint32_t>>();
        out.edges = empty;
        return true;
    }
    const std::uint64_t key = (static_cast<std::uint64_t>(from) << 34) | (static_cast<std::uint64_t>(to) << 2) | static_cast<std::uint64_t>(mode);
    if (const auto it = cache_.find(key); it != cache_.end() && it->second.version == version_) {
        out.edges = it->second.edges;
        return out.edges != nullptr;
    }

    ++searches_;
    if (++stamp_value_ == 0) { std::fill(stamp_.begin(), stamp_.end(), 0); stamp_value_ = 1; }
    const float max_speed = mode == TravelMode::Car ? 14.f : mode == TravelMode::Bike ? 4.6f : 1.38f;
    const Vec2 goal = map_.nodes[to].pos;
    auto h = [&](std::uint32_t n) { return distance(map_.nodes[n].pos, goal) / max_speed; };
    using Item = std::pair<float, std::uint32_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    g_[from] = 0;
    came_edge_[from] = kNone;
    stamp_[from] = stamp_value_;
    open.emplace(h(from), from);
    bool found = false;
    while (!open.empty()) {
        const auto [f, u] = open.top();
        open.pop();
        if (u == to) { found = true; break; }
        if (f - h(u) > g_[u] + 1e-3f) continue; // stale entry
        for (auto ei : map_.nodes[u].edges) {
            const float c = edge_cost(mode, ei);
            if (c < 0) continue;
            const auto v = map_.edges[ei].other(u);
            const float ng = g_[u] + c;
            if (stamp_[v] != stamp_value_ || ng < g_[v]) {
                stamp_[v] = stamp_value_;
                g_[v] = ng;
                came_edge_[v] = ei;
                open.emplace(ng + h(v), v);
            }
        }
    }
    std::shared_ptr<const std::vector<std::uint32_t>> result;
    if (found) {
        auto edges = std::make_shared<std::vector<std::uint32_t>>();
        for (auto n = to; n != from;) {
            const auto ei = came_edge_[n];
            edges->push_back(ei);
            n = map_.edges[ei].other(n);
        }
        std::reverse(edges->begin(), edges->end());
        result = std::move(edges);
    }
    if (cache_.size() >= kMaxCache) cache_.clear();
    cache_[key] = {version_, result};
    out.edges = result;
    return found;
}

} // namespace npc::world
