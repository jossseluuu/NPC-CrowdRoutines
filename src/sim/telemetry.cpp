#include "sim/telemetry.hpp"

#include <cmath>
#include <fstream>

namespace npc::sim {

namespace {
constexpr std::size_t kMaxSamples = 30 * 24 * 60; // 30 simulated days at one sample per minute
double r3(double v) { return std::round(v * 1000) / 1000; }
} // namespace

void Telemetry::sample(const SimState& s) {
    const auto& a = s.agents;
    const auto& cat = s.cat;
    Sample x;
    x.t = s.now();
    x.activity.assign(cat.activities.size(), 0);
    const int walking = cat.sub("walking"), talking = cat.sub("walking_talking"), strolling = cat.sub("strolling"),
              hurrying = cat.sub("hurrying"), cycling = cat.sub("cycling"), driving = cat.sub("driving"),
              stuck = cat.sub("stuck_in_traffic"), on_bus = cat.sub("on_bus"), waiting_bus = cat.sub("waiting_bus"),
              waiting_cross = cat.sub("waiting_crossing");
    double walk_speed_sum = 0, car_speed_sum = 0;
    std::uint32_t walkers_moving = 0, cars_moving = 0;
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        ++x.activity[a.activity[i]];
        const auto& n = a.needs[i];
        x.stress += n[data::Stress];
        x.fear += n[data::Fear];
        x.energy += n[data::Energy];
        x.hunger += n[data::Hunger];
        x.social += n[data::Social];
        x.fun += n[data::Fun];
        x.outdoor += a.outdoor[i];
        x.groups += a.group[i] != pop::kNone && a.group[i] == i;
        const int sub = a.substate[i];
        if (sub == walking || sub == talking || sub == strolling || sub == hurrying) {
            ++x.walking;
            if (a.speed[i] > 0) { walk_speed_sum += a.speed[i]; ++walkers_moving; }
        } else if (sub == cycling) ++x.cycling;
        else if (sub == driving || sub == stuck) {
            ++x.driving;
            if (a.speed[i] > 0) { car_speed_sum += a.speed[i]; ++cars_moving; }
        } else if (sub == on_bus) ++x.bus;
        else if (sub == waiting_bus || sub == waiting_cross) ++x.waiting;
        x.km += a.distance[i];
    }
    const float n = a.size() ? static_cast<float>(a.size()) : 1.f;
    x.stress /= n; x.fear /= n; x.energy /= n; x.hunger /= n; x.social /= n; x.fun /= n;
    x.km /= 1000.f;
    x.walk_speed = walkers_moving ? static_cast<float>(walk_speed_sum / walkers_moving) : 0.f;
    x.car_speed = cars_moving ? static_cast<float>(car_speed_sum / cars_moving) : 0.f;
    double load = 0;
    std::uint32_t used = 0;
    for (std::uint32_t e = 0; e < s.map.edges.size() && e < s.cars_on_edge.size(); ++e) {
        if (!s.map.edges[e].cars() || s.cars_on_edge[e] == 0) continue;
        const float lanes = s.map.edges[e].kind == world::RoadKind::Avenue ? 2.f : 1.f;
        load += s.cars_on_edge[e] / std::max(2.f, s.map.edges[e].length / 7.5f * lanes);
        ++used;
    }
    x.congestion = used ? static_cast<float>(load / used) : 0.f;
    for (const auto& e : s.events) x.events += e.phase == EventPhase::Active;
    x.weather = static_cast<std::uint8_t>(s.weather.kind);
    if (samples_.size() >= kMaxSamples) samples_.erase(samples_.begin(), samples_.begin() + static_cast<long>(kMaxSamples / 10));
    samples_.push_back(std::move(x));
}

json::Value Telemetry::series(const SimState& s, double since, std::size_t max_points) const {
    std::size_t first = 0;
    while (first < samples_.size() && samples_[first].t <= since) ++first;
    const std::size_t count = samples_.size() - first;
    const std::size_t stride = max_points > 0 && count > max_points ? (count + max_points - 1) / max_points : 1;
    json::Value out = json::Value::object();
    json::Value t = json::Value::array(), acts = json::Value::array();
    const char* fields[] = {"stress", "fear", "energy", "hunger", "social", "fun", "outdoor", "walking", "cycling", "driving",
                            "bus", "waiting", "groups", "walk_speed", "car_speed", "congestion", "km", "events", "weather"};
    std::vector<json::Value> cols(std::size(fields), json::Value::array());
    for (std::size_t a = 0; a < s.cat.activities.size(); ++a) acts.push_back(json::Value::array());
    for (std::size_t k = first; k < samples_.size(); k += stride) {
        const auto& x = samples_[k];
        t.push_back(x.t);
        for (std::size_t a = 0; a < x.activity.size() && a < acts.size(); ++a) acts.as_array_mut()[a].push_back(x.activity[a]);
        const double vals[] = {r3(x.stress), r3(x.fear), r3(x.energy), r3(x.hunger), r3(x.social), r3(x.fun), static_cast<double>(x.outdoor),
                               static_cast<double>(x.walking), static_cast<double>(x.cycling), static_cast<double>(x.driving),
                               static_cast<double>(x.bus), static_cast<double>(x.waiting), static_cast<double>(x.groups), r3(x.walk_speed),
                               r3(x.car_speed), r3(x.congestion), r3(x.km), static_cast<double>(x.events), static_cast<double>(x.weather)};
        for (std::size_t f = 0; f < std::size(fields); ++f) cols[f].push_back(vals[f]);
    }
    out.set("t", std::move(t));
    out.set("activity", std::move(acts));
    for (std::size_t f = 0; f < std::size(fields); ++f) out.set(fields[f], std::move(cols[f]));
    return out;
}

bool Telemetry::write_csv(const SimState& s, const std::string& path) const {
    std::ofstream out(path);
    if (!out) return false;
    out << "time,label";
    for (const auto& a : s.cat.activities) out << ",act_" << a.id;
    out << ",stress,fear,energy,hunger,social,fun,outdoor,walking,cycling,driving,bus,waiting,groups,walk_speed,car_speed,congestion,km,events,weather\n";
    for (const auto& x : samples_) {
        out << x.t << ',' << SimClock::format(x.t);
        for (auto c : x.activity) out << ',' << c;
        out << ',' << x.stress << ',' << x.fear << ',' << x.energy << ',' << x.hunger << ',' << x.social << ',' << x.fun << ',' << x.outdoor
            << ',' << x.walking << ',' << x.cycling << ',' << x.driving << ',' << x.bus << ',' << x.waiting << ',' << x.groups << ','
            << x.walk_speed << ',' << x.car_speed << ',' << x.congestion << ',' << x.km << ',' << x.events << ','
            << data::kWeatherNames[x.weather] << '\n';
    }
    return static_cast<bool>(out);
}

} // namespace npc::sim
