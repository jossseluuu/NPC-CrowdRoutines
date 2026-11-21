#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace npc_crowd {

struct Vec2 {
    float x{}, y{};
    Vec2& operator+=(Vec2 b) { x += b.x; y += b.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x+b.x, a.y+b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x-b.x, a.y-b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x*s, a.y*s}; }
inline float length_sq(Vec2 v) { return v.x*v.x + v.y*v.y; }

enum class Activity : std::uint8_t { Home, Sleep, Breakfast, Commute, Work, Study, Care, Eat, Leisure, Talk, Pursue, Flee, Evacuate };
enum class EventType : std::uint8_t { TrafficBlock, Rain, Alarm };
enum class Backend : std::uint8_t { Oop, Soa };
enum class Sex : std::uint8_t { Male, Female, Other };
enum class RoutineProfile : std::uint8_t { DayWorker, ShiftWorker, Student, Care, Retired, Flexible };

struct Agent {
    std::uint32_t id{};
    Vec2 position{}, velocity{}, home{}, workplace{}, destination{};
    float energy{1.f}, hunger{}, stress{}, social{}, bladder{}, comfort{.5f};
    float health{1.f}, fatigue{}, fear{}, curiosity{}, money{.5f}, temperature{.5f};
    float sociability{.5f}, risk_aversion{.5f}, patience{.5f}, conscientiousness{.5f};
    float wake_hour{7.f}, sleep_hour{22.f}, work_start_hour{8.f}, work_duration{8.f}, breakfast_hour{7.f};
    float breakfast_probability{.7f};
    std::uint16_t age{30}, workday_mask{0b0011111};
    std::uint8_t breakfast_venue{}, commute_mode{};
    Sex sex{Sex::Other};
    RoutineProfile profile{RoutineProfile::DayWorker};
    std::uint8_t job_type{};
    Activity activity{Activity::Home};
};

struct Event {
    EventType type{};
    Vec2 position{};
    float radius{30.f};
    std::uint32_t expires{};
};

struct Config {
    std::size_t population{5000};
    std::uint64_t seed{42};
    std::uint32_t ticks{1200};
    float speed{10.f};
    Backend backend{Backend::Soa};
};

struct Metrics {
    std::vector<double> tick_ms;
    std::size_t events{};
    double elapsed_ms{};
    double memory_bytes{};
    double percentile(double p) const {
        if(tick_ms.empty()) return 0.;
        auto v=tick_ms; std::sort(v.begin(),v.end());
        return v[std::min(v.size()-1,static_cast<std::size_t>(p*(v.size()-1)))];
    }
};
} // namespace npc_crowd
