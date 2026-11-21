#pragma once
// A* routing over the city road graph with per-mode costs, event closures and a route cache.

#include "world/city_map.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace npc::world {

enum class TravelMode : std::uint8_t { Walk, Bike, Bus, Car };
constexpr int kTravelModeCount = 4;
const char* travel_mode_name(TravelMode m);

/// Sequence of edges from a start node; nodes are derived on the fly.
struct Route {
    std::uint32_t start = 0;
    std::shared_ptr<const std::vector<std::uint32_t>> edges; // shared with the cache
    std::size_t size() const { return edges ? edges->size() : 0; }
    bool empty() const { return size() == 0; }
};

class Router {
public:
    explicit Router(const CityMap& map);

    /// Shortest-time route. Returns false if the destination is unreachable for that mode.
    bool route(std::uint32_t from, std::uint32_t to, TravelMode mode, Route& out);

    // Closures from events. Changing them invalidates cached routes lazily.
    void set_car_blocked(std::uint32_t edge, bool blocked);
    void set_hazard(std::uint32_t edge, float extra_seconds); // pedestrians avoid but can pass
    void clear_closures();
    bool car_blocked(std::uint32_t edge) const { return car_blocked_[edge] != 0; }

    static float mode_speed(TravelMode mode, const Edge& e);
    std::size_t cache_size() const { return cache_.size(); }
    std::uint64_t searches() const { return searches_; }

private:
    float edge_cost(TravelMode mode, std::uint32_t edge) const;

    const CityMap& map_;
    std::vector<std::uint8_t> car_blocked_;
    std::vector<float> hazard_;
    std::uint32_t version_ = 0;
    struct Cached { std::uint32_t version; std::shared_ptr<const std::vector<std::uint32_t>> edges; };
    std::unordered_map<std::uint64_t, Cached> cache_;
    std::uint64_t searches_ = 0;
    // Scratch buffers reused across searches.
    std::vector<float> g_;
    std::vector<std::uint32_t> came_edge_, stamp_;
    std::uint32_t stamp_value_ = 0;
};

} // namespace npc::world
