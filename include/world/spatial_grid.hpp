#pragma once

#include "core/types.hpp"
#include <cmath>
#include <unordered_map>

namespace npc_crowd {
class SpatialGrid {
public:
    explicit SpatialGrid(float cell=12.f) : cell_(cell) {}
    void rebuild(const std::vector<Vec2>& positions) {
        cells_.clear();
        for(std::size_t i=0;i<positions.size();++i) cells_[key(positions[i])].push_back(static_cast<std::uint32_t>(i));
    }
    template<class F> void nearby(Vec2 p,float radius,F&& callback) const {
        const int r=static_cast<int>(std::ceil(radius/cell_));
        const int cx=static_cast<int>(std::floor(p.x/cell_)),cy=static_cast<int>(std::floor(p.y/cell_));
        const float rr=radius*radius;
        for(int y=cy-r;y<=cy+r;++y) for(int x=cx-r;x<=cx+r;++x) {
            const auto it=cells_.find(pack(x,y)); if(it==cells_.end()) continue;
            for(auto index:it->second) callback(index,rr);
        }
    }
private:
    static std::int64_t pack(int x,int y) {
        const auto hi=static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
        const auto lo=static_cast<std::uint32_t>(y);
        return static_cast<std::int64_t>((hi<<32)|lo);
    }
    std::int64_t key(Vec2 p) const { return pack(static_cast<int>(std::floor(p.x/cell_)),static_cast<int>(std::floor(p.y/cell_))); }
    float cell_;
    std::unordered_map<std::int64_t,std::vector<std::uint32_t>> cells_;
};
} // namespace npc_crowd
