#pragma once
// Simulation clock. One tick is a fixed step of simulated time (default 1 s).

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace npc {

constexpr double kSecondsPerDay = 86400.0;

struct SimClock {
    std::uint64_t tick = 0;
    double step_seconds = 1.0;
    double start_seconds = 8.0 * 3600.0; // offset of tick 0 inside day 0
    int start_weekday = 0;               // 0 = Monday

    double seconds() const { return start_seconds + static_cast<double>(tick) * step_seconds; }
    int day() const { return static_cast<int>(seconds() / kSecondsPerDay); }
    double hour() const { const double s = seconds(); return (s - day() * kSecondsPerDay) / 3600.0; }
    int weekday() const { return (start_weekday + day()) % 7; }
    bool is_weekend() const { return weekday() >= 5; }

    /// Ticks needed to reach a given absolute simulated second (rounded up).
    std::uint64_t tick_at(double absolute_seconds) const {
        const double d = (absolute_seconds - start_seconds) / step_seconds;
        return d <= 0 ? 0 : static_cast<std::uint64_t>(d + 0.999999);
    }

    std::string label() const { return format(seconds()); }

    /// "d2 08:05" style label for an absolute simulated second.
    static std::string format(double absolute_seconds) {
        const int d = static_cast<int>(absolute_seconds / kSecondsPerDay);
        const int rem = static_cast<int>(absolute_seconds - d * kSecondsPerDay);
        char buf[32];
        std::snprintf(buf, sizeof buf, "d%d %02d:%02d", d, rem / 3600, (rem / 60) % 60);
        return buf;
    }

    /// Parses "d2 08:05", "08:05" (day 0) or a plain number of hours. Returns -1 on error.
    static double parse(std::string_view s) {
        int d = 0, h = 0, m = 0;
        std::string tmp(s);
        if (std::sscanf(tmp.c_str(), "d%d %d:%d", &d, &h, &m) == 3) return d * kSecondsPerDay + h * 3600.0 + m * 60.0;
        if (std::sscanf(tmp.c_str(), "%d:%d", &h, &m) == 2) return h * 3600.0 + m * 60.0;
        double hours = 0;
        if (std::sscanf(tmp.c_str(), "%lf", &hours) == 1) return hours * 3600.0;
        return -1.0;
    }
};

/// True if hour h lies in [start, end), handling windows that cross midnight.
constexpr bool hour_in_window(double h, double start, double end) {
    return start <= end ? (h >= start && h < end) : (h >= start || h < end);
}

} // namespace npc
