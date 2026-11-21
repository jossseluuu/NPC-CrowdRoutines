#include "sim/simulation.hpp"

#include "behavior/behavior.hpp"
#include "behavior/movement.hpp"
#include "events/event_system.hpp"
#include "population/generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace npc::sim {

using pop::kNone;

namespace {
constexpr int kRunFormatVersion = 1;
constexpr const char* kEngineVersion = "0.3.0";

double round2(double v) { return std::round(v * 100) / 100; }

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int k = 0; k < 4; ++k) out.push_back(static_cast<std::uint8_t>(v >> (8 * k)));
}
void put_f32(std::vector<std::uint8_t>& out, float f) {
    std::uint32_t v;
    std::memcpy(&v, &f, 4);
    put_u32(out, v);
}
void put_f64(std::vector<std::uint8_t>& out, double d) {
    std::uint64_t v;
    std::memcpy(&v, &d, 8);
    for (int k = 0; k < 8; ++k) out.push_back(static_cast<std::uint8_t>(v >> (8 * k)));
}
} // namespace

Simulation::Simulation(const data::Catalog& catalog, const world::MapLibrary& maps) : cat_(catalog), maps_(maps), state_(catalog) {
    auto r = json::parse_file(catalog.data_dir + "/names.json");
    if (r.ok()) names_ = std::move(r.value);
}

bool Simulation::reset(const Config& config, std::string& error) {
    SimState fresh(cat_);
    fresh.config = config;
    fresh.clock = SimClock{};
    fresh.clock.step_seconds = config.step_seconds;
    fresh.clock.start_seconds = config.start_hour * 3600.0;
    fresh.clock.start_weekday = config.start_weekday;
    fresh.rng_behavior = Rng(config.seed, "behavior");
    fresh.rng_events = Rng(config.seed, "events");
    fresh.rng_social = Rng(config.seed, "social");

    if (!config.map_file.empty()) {
        auto r = json::parse_file(config.map_file);
        if (!r.ok()) { error = r.error; return false; }
        if (!world::CityMap::from_json(r.value, cat_, fresh.map, error)) { error = config.map_file + ": " + error; return false; }
        if (!fresh.map.finalize(cat_, error)) { error = config.map_file + ": " + error; return false; }
    } else {
        world::MapSpec spec{config.map_template, config.map_size, config.map_seed ? config.map_seed : config.seed, ""};
        if (!world::generate_map(spec, maps_, cat_, fresh.map, error)) return false;
    }
    std::size_t population = config.population > 0 ? static_cast<std::size_t>(config.population)
                                                    : static_cast<std::size_t>(std::max(500, fresh.map.suggested_population));
    population = std::min<std::size_t>(population, 60000);
    fresh.config.population = static_cast<int>(population);
    fresh.router = std::make_unique<world::Router>(fresh.map);
    fresh.occupancy.assign(fresh.map.buildings.size(), 0);
    fresh.closed.assign(fresh.map.buildings.size(), 0);
    fresh.attraction.assign(fresh.map.buildings.size(), 0.f);
    movement::init_transport(fresh);
    pop::generate_population(fresh, population, config.seed);
    for (std::uint32_t i = 0; i < fresh.agents.size(); ++i) {
        ++fresh.occupancy[fresh.agents.home[i]];
        fresh.agents.pos[i] = behavior::interior_point(fresh, i, fresh.agents.home[i]);
        pop::build_agenda(fresh, i, 0);
        fresh.agents.record_state(i, fresh.now());
    }
    // Residents whose personal sleep window covers the start time begin asleep.
    for (std::uint32_t i = 0; i < fresh.agents.size(); ++i)
        if (hour_in_window(fresh.hour(), fresh.agents.sleep[i], fresh.agents.wake[i])) {
            fresh.agents.activity[i] = static_cast<std::uint8_t>(cat_.act_sleeping);
            fresh.agents.substate[i] = static_cast<std::uint8_t>(cat_.sub("night_sleep"));
        }
    // SimState holds references, so rebuild in place.
    state_.~SimState();
    new (&state_) SimState(std::move(fresh));
    state_.router = std::make_unique<world::Router>(state_.map); // the router must reference the map it now lives with
    telemetry_.clear();
    incoming_.clear();
    scheduled_.clear();
    scheduled_next_ = 0;
    log_.clear();
    scenario_name_.clear();
    telemetry_.sample(state_);
    return true;
}

bool Simulation::load_scenario(const json::Value& sc, std::string& error) {
    if (sc.str("format") != "npc-scenario") { error = "not a scenario (format must be \"npc-scenario\")"; return false; }
    Config cfg = Config::from_json(sc["config"]);
    if (!cfg.map_file.empty() && cfg.map_file.front() != '/' && cfg.map_file.find(':') == std::string::npos)
        cfg.map_file = cat_.data_dir + "/" + cfg.map_file; // relative to data/
    if (!reset(cfg, error)) return false;
    scenario_name_ = sc.str("name", cfg.name);
    for (const auto& e : sc["events"].as_array()) {
        const double at = SimClock::parse(e.str("at", "0"));
        if (at < 0) { error = "scenario event has an invalid time '" + e.str("at") + "' (use \"d0 08:30\")"; return false; }
        if (cat_.event_index(e.str("event")) < 0) { error = "scenario uses unknown event '" + e.str("event") + "'"; return false; }
        json::Value cmd = e;
        cmd.set("type", "inject_event");
        scheduled_.push_back({state_.clock.tick_at(at), std::move(cmd)});
    }
    std::stable_sort(scheduled_.begin(), scheduled_.end(), [](const Command& a, const Command& b) { return a.tick < b.tick; });
    return true;
}

json::Value Simulation::save_run() const {
    json::Value v = json::Value::object();
    v.set("format", "npc-run");
    v.set("version", kRunFormatVersion);
    v.set("engine", kEngineVersion);
    v.set("scenario", scenario_name_);
    v.set("config", state_.config.to_json());
    json::Value cmds = json::Value::array();
    for (const auto& c : log_) {
        json::Value j = json::Value::object();
        j.set("tick", static_cast<double>(c.tick));
        j.set("command", c.body);
        cmds.push_back(std::move(j));
    }
    // Scheduled commands not yet reached are kept so a replay continues the scenario.
    for (std::size_t k = scheduled_next_; k < scheduled_.size(); ++k) {
        json::Value j = json::Value::object();
        j.set("tick", static_cast<double>(scheduled_[k].tick));
        j.set("command", scheduled_[k].body);
        cmds.push_back(std::move(j));
    }
    v.set("commands", std::move(cmds));
    v.set("end_tick", static_cast<double>(state_.clock.tick));
    v.set("end_time", state_.clock.label());
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(checksum()));
    v.set("checksum", buf);
    return v;
}

bool Simulation::load_run(const json::Value& run, std::string& error, bool replay, bool* checksum_ok) {
    if (run.str("format") != "npc-run") { error = "not a saved run (format must be \"npc-run\")"; return false; }
    if (run.integer("version", 1) > kRunFormatVersion) { error = "saved run was made by a newer engine"; return false; }
    if (!reset(Config::from_json(run["config"]), error)) return false;
    scenario_name_ = run.str("scenario");
    for (const auto& c : run["commands"].as_array())
        scheduled_.push_back({static_cast<std::uint64_t>(c.num("tick", 0)), c["command"]});
    std::stable_sort(scheduled_.begin(), scheduled_.end(), [](const Command& a, const Command& b) { return a.tick < b.tick; });
    if (replay) {
        const auto end = static_cast<std::uint64_t>(run.num("end_tick", 0));
        while (state_.clock.tick < end) step();
        char buf[32];
        std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(checksum()));
        if (checksum_ok) *checksum_ok = run.str("checksum") == buf;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
bool Simulation::apply(const json::Value& c, std::string& error) {
    const std::string type = c.str("type");
    auto& s = state_;
    if (type == "inject_event") {
        const int et = cat_.event_index(c.str("event"));
        if (et < 0) { error = "unknown event '" + c.str("event") + "'"; return false; }
        const Vec2 pos = c.contains("x") ? Vec2{c.flt("x", -1), c.flt("y", -1)} : Vec2{-1, -1};
        events::spawn(s, et, pos, true, {}, c.flt("radius", -1), c.flt("duration", -1), c.flt("severity", -1));
    } else if (type == "end_event") {
        events::end_event(s, static_cast<std::int32_t>(c.integer("id", -1)));
    } else if (type == "set_chaos") {
        s.config.chaos = std::clamp(c.flt("value", 1), 0.f, 10.f);
    } else if (type == "set_random_events") {
        s.config.random_events = c.boolean("value", true);
    } else {
        error = "unknown command '" + type + "'";
        return false;
    }
    log_.push_back({s.clock.tick, c});
    return true;
}

void Simulation::step() {
    auto& s = state_;
    // Commands are applied at tick boundaries so a replay reproduces them exactly.
    while (scheduled_next_ < scheduled_.size() && scheduled_[scheduled_next_].tick <= s.clock.tick) {
        std::string err;
        apply(scheduled_[scheduled_next_].body, err);
        ++scheduled_next_;
    }
    while (!incoming_.empty()) {
        std::string err;
        apply(incoming_.front(), err);
        incoming_.pop_front();
    }
    const double before = s.now();
    ++s.clock.tick;
    const float dt = s.dt();
    events::update(s, dt);
    behavior::update_needs(s, dt);
    const double now = s.now();
    for (std::uint32_t i = 0; i < s.agents.size(); ++i)
        if (s.agents.next_decision[i] <= now) behavior::decide(s, i);
    movement::step(s, dt);
    if (s.clock.tick % 4 == 0) behavior::encounters(s);
    if (s.clock.tick % 30 == 0) behavior::refresh_substates(s);
    if (std::floor(now / 60.0) != std::floor(before / 60.0)) telemetry_.sample(s);
}

// ---------------------------------------------------------------------------------------------
std::uint64_t Simulation::checksum() const {
    const auto& a = state_.agents;
    std::uint64_t h = 0xcbf29ce484222325ull;
    auto mix = [&](std::uint64_t v) { h ^= v; h *= 0x100000001b3ull; };
    mix(state_.clock.tick);
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        mix(static_cast<std::uint64_t>(std::llround(a.pos[i].x * 10)));
        mix(static_cast<std::uint64_t>(std::llround(a.pos[i].y * 10)));
        mix(a.activity[i]);
        mix(a.substate[i]);
        mix(a.location[i]);
        for (float n : a.needs[i]) mix(static_cast<std::uint64_t>(std::llround(n * 1000)));
    }
    mix(static_cast<std::uint64_t>(state_.next_event_id));
    return h;
}

std::string Simulation::resident_name(std::uint32_t i) const {
    const auto& a = state_.agents;
    if (i >= a.size()) return {};
    const auto& pool = a.sex[i] == pop::Sex::Male ? names_["male"] : a.sex[i] == pop::Sex::Female ? names_["female"] : names_["neutral"];
    const auto& sur = names_["surnames"];
    if (pool.size() == 0 || sur.size() == 0) return "#" + std::to_string(i);
    const auto h = hash_combine(state_.config.seed, i);
    const auto hh = hash_combine(state_.config.seed ^ 0x5bd1e995, a.household[i]);
    return pool[h % pool.size()].as_string() + " " + sur[hh % sur.size()].as_string() + " " + sur[(h >> 20) % sur.size()].as_string();
}

std::string Simulation::building_label(std::uint32_t b) const {
    if (b == kNone || b >= state_.map.buildings.size()) return {};
    const auto& bd = state_.map.buildings[b];
    return cat_.building_types[static_cast<std::size_t>(bd.type)].id + " #" + std::to_string(b);
}

json::Value Simulation::info_json() const {
    const auto& s = state_;
    json::Value v = json::Value::object();
    v.set("engine", kEngineVersion);
    v.set("config", s.config.to_json());
    v.set("scenario", scenario_name_);
    v.set("population", static_cast<double>(s.agents.size()));
    json::Value m = json::Value::object();
    m.set("name", s.map.name);
    m.set("template", s.map.template_id);
    m.set("size", s.map.size_id);
    m.set("width", s.map.width);
    m.set("height", s.map.height);
    m.set("buildings", static_cast<double>(s.map.buildings.size()));
    m.set("nodes", static_cast<double>(s.map.nodes.size()));
    m.set("edges", static_cast<double>(s.map.edges.size()));
    m.set("households", static_cast<double>(s.map.total_households()));
    m.set("jobs", static_cast<double>(s.map.total_jobs()));
    m.set("bus_lines", static_cast<double>(s.map.bus_lines.size()));
    m.set("buses", static_cast<double>(s.buses.size()));
    v.set("map", std::move(m));
    return v;
}

json::Value Simulation::state_json(std::size_t recent) const {
    const auto& s = state_;
    const auto& a = s.agents;
    json::Value v = json::Value::object();
    json::Value clock = json::Value::object();
    clock.set("tick", static_cast<double>(s.clock.tick));
    clock.set("seconds", s.now());
    clock.set("label", s.clock.label());
    clock.set("day", s.clock.day());
    clock.set("hour", round2(s.hour()));
    clock.set("weekday", s.clock.weekday());
    v.set("clock", std::move(clock));
    v.set("population", static_cast<double>(a.size()));
    std::vector<std::uint32_t> acts(cat_.activities.size(), 0), subs(cat_.substates.size(), 0);
    double stress = 0, fear = 0;
    std::uint32_t outdoor = 0, moving = 0, emergency = 0, groups = 0;
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        ++acts[a.activity[i]];
        ++subs[a.substate[i]];
        stress += a.needs[i][data::Stress];
        fear += a.needs[i][data::Fear];
        outdoor += a.outdoor[i];
        moving += a.move[i].active;
        emergency += a.activity[i] == cat_.act_emergency;
        groups += a.group[i] != kNone && a.group[i] == i;
    }
    json::Value ja = json::Value::array(), js = json::Value::array();
    for (auto c : acts) ja.push_back(c);
    for (auto c : subs) js.push_back(c);
    v.set("activities", std::move(ja));
    v.set("substates", std::move(js));
    const double n = a.size() ? static_cast<double>(a.size()) : 1.0;
    json::Value st = json::Value::object();
    st.set("stress", round2(stress / n));
    st.set("fear", round2(fear / n));
    st.set("outdoor", outdoor);
    st.set("moving", moving);
    st.set("emergency", emergency);
    st.set("groups", groups);
    std::uint32_t riders = 0;
    for (const auto& b : s.buses) riders += b.passengers;
    st.set("bus_riders", riders);
    if (!telemetry_.samples().empty()) {
        const auto& last = telemetry_.samples().back();
        st.set("congestion", round2(last.congestion));
        st.set("car_speed", round2(last.car_speed * 3.6));
        st.set("walk_speed", round2(last.walk_speed * 3.6));
    }
    v.set("stats", std::move(st));
    v.set("weather", data::kWeatherNames[static_cast<std::size_t>(s.weather.kind)]);
    v.set("chaos", s.config.chaos);
    v.set("random_events", s.config.random_events);
    json::Value evs = json::Value::array();
    for (const auto& e : s.events) evs.push_back(events::event_json(s, e));
    v.set("events", std::move(evs));
    json::Value hist = json::Value::array();
    const std::size_t from = s.event_history.size() > recent ? s.event_history.size() - recent : 0;
    for (std::size_t k = s.event_history.size(); k > from; --k) hist.push_back(events::event_json(s, s.event_history[k - 1]));
    v.set("event_history", std::move(hist));
    v.set("commands", static_cast<double>(log_.size()));
    v.set("scenario", scenario_name_);
    return v;
}

json::Value Simulation::residents_json() const {
    const auto& a = state_.agents;
    json::Value v = json::Value::object();
    json::Value profile = json::Value::array(), age = json::Value::array(), sex = json::Value::array(), mode = json::Value::array(),
                district = json::Value::array();
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        profile.push_back(a.profile[i]);
        age.push_back(a.age[i]);
        sex.push_back(static_cast<int>(a.sex[i]));
        mode.push_back(static_cast<int>(a.commute[i]));
        district.push_back(state_.map.buildings[a.home[i]].district);
    }
    v.set("profile", std::move(profile));
    v.set("age", std::move(age));
    v.set("sex", std::move(sex));
    v.set("mode", std::move(mode));
    v.set("district", std::move(district));
    return v;
}

json::Value Simulation::map_json() const {
    return state_.map.to_json(cat_);
}

std::vector<std::uint8_t> Simulation::frame_binary() const {
    // Layout (little endian): "NPCF" u32 version | f64 seconds | u32 n | n * {f32 x, f32 y, u8 act, u8 sub, u8 flags, u8 stress}
    //                         | u32 buses | buses * {f32 x, f32 y, u16 line, u16 passengers} | u32 edges | edges * u8 load
    const auto& s = state_;
    const auto& a = s.agents;
    std::vector<std::uint8_t> out;
    out.reserve(24 + a.size() * 12 + s.buses.size() * 12 + s.map.edges.size());
    out.insert(out.end(), {'N', 'P', 'C', 'F'});
    put_u32(out, 1);
    put_f64(out, s.now());
    put_u32(out, static_cast<std::uint32_t>(a.size()));
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        put_f32(out, a.pos[i].x);
        put_f32(out, a.pos[i].y);
        out.push_back(a.activity[i]);
        out.push_back(a.substate[i]);
        const auto& mv = a.move[i];
        std::uint8_t flags = 0;
        if (a.outdoor[i]) flags |= 1;
        if (mv.active) flags |= 2;
        if (a.group[i] != kNone) flags |= 4;
        if (a.activity[i] == cat_.act_emergency) flags |= 8;
        if (mv.active && mv.legs[mv.leg].kind == pop::LegKind::Drive) flags |= 16;
        if (mv.active && mv.legs[mv.leg].kind == pop::LegKind::Ride) flags |= 32;
        if (mv.active && mv.legs[mv.leg].kind == pop::LegKind::Bike) flags |= 64;
        out.push_back(flags);
        out.push_back(static_cast<std::uint8_t>(std::lround(a.needs[i][data::Stress] * 255.f)));
    }
    put_u32(out, static_cast<std::uint32_t>(s.buses.size()));
    for (const auto& b : s.buses) {
        put_f32(out, b.pos.x);
        put_f32(out, b.pos.y);
        out.push_back(static_cast<std::uint8_t>(b.line & 0xFF));
        out.push_back(static_cast<std::uint8_t>(b.line >> 8));
        out.push_back(static_cast<std::uint8_t>(b.passengers & 0xFF));
        out.push_back(static_cast<std::uint8_t>(b.passengers >> 8));
    }
    put_u32(out, static_cast<std::uint32_t>(s.map.edges.size()));
    for (std::uint32_t e = 0; e < s.map.edges.size(); ++e) {
        const auto& edge = s.map.edges[e];
        float load = 0;
        if (edge.cars() && e < s.cars_on_edge.size()) {
            const float lanes = edge.kind == world::RoadKind::Avenue ? 2.f : 1.f;
            load = s.cars_on_edge[e] / std::max(2.f, edge.length / 7.5f * lanes);
        }
        std::uint8_t code = static_cast<std::uint8_t>(std::min(250.f, load * 100.f));
        if (s.router && s.router->car_blocked(e)) code = 255; // closed
        out.push_back(code);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
bool Simulation::matches(std::uint32_t i, const json::Value& r) const {
    const auto& a = state_.agents;
    // Nested groups: {"any": [rules]} / {"all": [rules]} (used by the population tree + filter panel combination).
    if (r.contains("any")) {
        for (const auto& sub : r["any"].as_array())
            if (matches(i, sub) != sub.boolean("negate", false)) return true;
        return r["any"].size() == 0;
    }
    if (r.contains("all")) {
        for (const auto& sub : r["all"].as_array())
            if (matches(i, sub) == sub.boolean("negate", false)) return false;
        return true;
    }
    const std::string field = r.str("field");
    const auto& val = r["value"];
    auto in_range = [&](double x) {
        if (r.contains("min") && x < r.num("min", 0)) return false;
        if (r.contains("max") && x > r.num("max", 0)) return false;
        return true;
    };
    if (field == "activity") return cat_.activities[a.activity[i]].id == val.as_string();
    if (field == "substate") return cat_.substates[a.substate[i]].id == val.as_string();
    if (field == "profile") return cat_.profiles[a.profile[i]].id == val.as_string();
    if (field == "archetype") return cat_.archetypes[a.archetype[i]].id == val.as_string() || cat_.archetypes[a.archetype2[i]].id == val.as_string();
    if (field == "district") return state_.map.buildings[a.home[i]].district == val.as_int(-1);
    if (field == "sex") return (val.as_string() == "male" && a.sex[i] == pop::Sex::Male) || (val.as_string() == "female" && a.sex[i] == pop::Sex::Female) ||
                               (val.as_string() == "other" && a.sex[i] == pop::Sex::Other);
    if (field == "age") return in_range(a.age[i]);
    if (field == "need" || field == "trait") {
        const std::string name = r.str("name");
        const auto& names = field == "need" ? std::vector<const char*>(data::kNeedNames.begin(), data::kNeedNames.end())
                                            : std::vector<const char*>(data::kTraitNames.begin(), data::kTraitNames.end());
        for (std::size_t k = 0; k < names.size(); ++k)
            if (name == names[k]) return in_range(field == "need" ? a.needs[i][k] : a.traits[i][k]);
        return false;
    }
    if (field == "mode") return world::travel_mode_name(a.commute[i]) == val.as_string();
    if (field == "place") return a.location[i] != kNone && cat_.categories[static_cast<std::size_t>(state_.map.buildings[a.location[i]].category)] == val.as_string();
    if (field == "work") return a.work[i] != kNone && cat_.categories[static_cast<std::size_t>(state_.map.buildings[a.work[i]].category)] == val.as_string();
    if (field == "outdoors") return (a.outdoor[i] != 0) == val.as_bool(true);
    if (field == "moving") return a.move[i].active == val.as_bool(true);
    if (field == "group") return (a.group[i] != kNone) == val.as_bool(true);
    if (field == "emergency") return (a.activity[i] == cat_.act_emergency) == val.as_bool(true);
    if (field == "knows_event") {
        if (val.is_string() && val.as_string() == "any") return a.known_events[i][0] >= 0;
        return a.knows(i, static_cast<std::int32_t>(val.as_int(-1)));
    }
    if (field == "relation") {
        for (std::size_t k = a.rel_begin(i); k < a.rel_end(i); ++k)
            if (pop::rel_kind_name(a.rels[k].kind) == val.as_string()) return true;
        return false;
    }
    if (field == "relations") return in_range(static_cast<double>(a.rel_end(i) - a.rel_begin(i)));
    if (field == "distance_km") return in_range(a.distance[i] / 1000.0);
    if (field == "id") return i == static_cast<std::uint32_t>(val.as_int(-1));
    return true; // unknown fields do not filter
}

std::vector<std::uint8_t> Simulation::query(const json::Value& filter, std::size_t* count) const {
    const auto& a = state_.agents;
    std::vector<std::uint8_t> bits((a.size() + 7) / 8, 0);
    const bool any = filter.str("logic", "all") == "any";
    const auto& rules = filter["rules"].as_array();
    std::size_t n = 0;
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        bool ok = rules.empty() ? true : !any;
        for (const auto& r : rules) {
            const bool m = matches(i, r) != r.boolean("negate", false);
            if (any && m) { ok = true; break; }
            if (!any && !m) { ok = false; break; }
        }
        if (ok) { bits[i / 8] |= static_cast<std::uint8_t>(1u << (i % 8)); ++n; }
    }
    if (count) *count = n;
    return bits;
}

std::string Simulation::export_csv(const json::Value& filter) const {
    const auto& a = state_.agents;
    const auto bits = query(filter);
    std::ostringstream out;
    out << "id,name,age,sex,profile,archetype,archetype2,activity,substate,place,home_district,commute,work,stress,energy,hunger,"
           "social,fun,fear,relations,known_events,distance_km,x,y\n";
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        if (!(bits[i / 8] & (1u << (i % 8)))) continue;
        int known = 0;
        for (auto e : a.known_events[i]) known += e >= 0;
        out << i << ",\"" << resident_name(i) << "\"," << static_cast<int>(a.age[i]) << ','
            << (a.sex[i] == pop::Sex::Male ? "male" : a.sex[i] == pop::Sex::Female ? "female" : "other") << ','
            << cat_.profiles[a.profile[i]].id << ',' << cat_.archetypes[a.archetype[i]].id << ',' << cat_.archetypes[a.archetype2[i]].id << ','
            << cat_.activities[a.activity[i]].id << ',' << cat_.substates[a.substate[i]].id << ',' << building_label(a.location[i]) << ','
            << state_.map.districts[static_cast<std::size_t>(state_.map.buildings[a.home[i]].district)].name << ','
            << world::travel_mode_name(a.commute[i]) << ',' << building_label(a.work[i]) << ',' << a.needs[i][data::Stress] << ','
            << a.needs[i][data::Energy] << ',' << a.needs[i][data::Hunger] << ',' << a.needs[i][data::Social] << ','
            << a.needs[i][data::Fun] << ',' << a.needs[i][data::Fear] << ',' << (a.rel_end(i) - a.rel_begin(i)) << ',' << known << ','
            << a.distance[i] / 1000.f << ',' << a.pos[i].x << ',' << a.pos[i].y << '\n';
    }
    return out.str();
}

json::Value Simulation::agent_json(std::uint32_t i) const {
    const auto& s = state_;
    const auto& a = s.agents;
    if (i >= a.size()) return {};
    json::Value v = json::Value::object();
    v.set("id", i);
    v.set("name", resident_name(i));
    v.set("age", a.age[i]);
    v.set("sex", a.sex[i] == pop::Sex::Male ? "male" : a.sex[i] == pop::Sex::Female ? "female" : "other");
    v.set("profile", cat_.profiles[a.profile[i]].id);
    v.set("archetype", cat_.archetypes[a.archetype[i]].id);
    v.set("archetype2", cat_.archetypes[a.archetype2[i]].id);
    v.set("activity", cat_.activities[a.activity[i]].id);
    v.set("substate", cat_.substates[a.substate[i]].id);
    v.set("since", a.since[i]);
    v.set("since_label", SimClock::format(a.since[i]));
    v.set("x", round2(a.pos[i].x));
    v.set("y", round2(a.pos[i].y));
    v.set("outdoor", a.outdoor[i] != 0);
    auto building = [&](std::uint32_t b) {
        if (b == kNone) return json::Value();
        json::Value j = json::Value::object();
        const auto& bd = s.map.buildings[b];
        j.set("id", b);
        j.set("type", cat_.building_types[static_cast<std::size_t>(bd.type)].id);
        j.set("district", s.map.districts[static_cast<std::size_t>(bd.district)].name);
        j.set("x", round2(bd.center.x));
        j.set("y", round2(bd.center.y));
        return j;
    };
    v.set("location", building(a.location[i]));
    v.set("home", building(a.home[i]));
    v.set("work", building(a.work[i]));
    v.set("commute", world::travel_mode_name(a.commute[i]));
    v.set("distance_km", round2(a.distance[i] / 1000.0));
    json::Value traits = json::Value::object(), needs = json::Value::object();
    for (std::size_t k = 0; k < data::kTraitCount; ++k) traits.set(data::kTraitNames[k], round2(a.traits[i][k]));
    for (std::size_t k = 0; k < data::kNeedCount; ++k) needs.set(data::kNeedNames[k], round2(a.needs[i][k]));
    v.set("traits", std::move(traits));
    v.set("needs", std::move(needs));
    json::Value schedule = json::Value::object();
    schedule.set("wake", round2(a.wake[i]));
    schedule.set("sleep", round2(a.sleep[i]));
    schedule.set("work_start", round2(a.work_start[i]));
    schedule.set("work_hours", round2(a.work_duration[i]));
    json::Value days = json::Value::array();
    for (int d = 0; d < 7; ++d) days.push_back(((a.workdays[i] >> d) & 1u) != 0);
    schedule.set("workdays", std::move(days));
    v.set("schedule", std::move(schedule));
    json::Value agenda = json::Value::array();
    for (const auto& it : a.agenda[i]) {
        json::Value j = json::Value::object();
        j.set("start", round2(it.start));
        j.set("end", round2(it.end));
        j.set("activity", cat_.activities[it.activity].id);
        j.set("hard", it.hard);
        j.set("place", building(it.building));
        agenda.push_back(std::move(j));
    }
    v.set("agenda", std::move(agenda));
    json::Value history = json::Value::array();
    for (int k = 0; k < pop::kHistory; ++k) {
        const auto& h = a.history[i * pop::kHistory + static_cast<std::size_t>((a.history_head[i] + pop::kHistory - 1 - k) % pop::kHistory)];
        if (h.t < 0) break;
        json::Value j = json::Value::object();
        j.set("t", h.t);
        j.set("label", SimClock::format(h.t));
        j.set("activity", cat_.activities[h.activity].id);
        j.set("substate", cat_.substates[h.substate].id);
        j.set("place", building(h.building));
        history.push_back(std::move(j));
    }
    v.set("history", std::move(history));
    json::Value actions = json::Value::array();
    for (int k = 0; k < pop::kActions; ++k) {
        const auto& e = a.actions[i * pop::kActions + static_cast<std::size_t>((a.actions_head[i] + pop::kActions - 1 - k) % pop::kActions)];
        if (e.t < 0) break;
        json::Value j = json::Value::object();
        j.set("t", e.t);
        j.set("label", SimClock::format(e.t));
        j.set("kind", pop::action_kind_name(e.kind));
        switch (e.kind) {
            case pop::ActionKind::Chatted: case pop::ActionKind::Argued: case pop::ActionKind::JoinedGroup: case pop::ActionKind::Pursued:
                if (e.a != kNone) { j.set("other", e.a); j.set("other_name", resident_name(e.a)); }
                break;
            case pop::ActionKind::ToldAbout:
                if (e.a != kNone) { j.set("other", e.a); j.set("other_name", resident_name(e.a)); }
                if (e.b != kNone) {
                    j.set("event", e.b);
                    const auto* ev = s.find_event(static_cast<std::int32_t>(e.b));
                    if (!ev) for (const auto& h : s.event_history) if (h.id == static_cast<std::int32_t>(e.b)) ev = &h;
                    if (ev) j.set("event_type", cat_.events[static_cast<std::size_t>(ev->type)].id);
                }
                break;
            case pop::ActionKind::Arrived:
                j.set("place", building(e.a));
                break;
            case pop::ActionKind::PerceivedEvent: case pop::ActionKind::Evacuated: case pop::ActionKind::Fled: case pop::ActionKind::Helped:
            case pop::ActionKind::Watched: case pop::ActionKind::Sheltered:
                if (e.a != kNone) {
                    j.set("event", e.a);
                    const auto* ev = s.find_event(static_cast<std::int32_t>(e.a));
                    if (!ev) for (const auto& h : s.event_history) if (h.id == static_cast<std::int32_t>(e.a)) ev = &h;
                    if (ev) j.set("event_type", cat_.events[static_cast<std::size_t>(ev->type)].id);
                }
                break;
            case pop::ActionKind::Boarded:
                if (e.a != kNone && e.a < s.map.bus_lines.size()) j.set("line", s.map.bus_lines[e.a].name);
                break;
            default: break;
        }
        actions.push_back(std::move(j));
    }
    v.set("actions", std::move(actions));
    json::Value rels = json::Value::array();
    for (std::size_t k = a.rel_begin(i); k < a.rel_end(i); ++k) {
        const auto& r = a.rels[k];
        json::Value j = json::Value::object();
        j.set("id", r.other);
        j.set("name", resident_name(r.other));
        j.set("kind", pop::rel_kind_name(r.kind));
        j.set("bond", round2(r.bond));
        j.set("activity", cat_.activities[a.activity[r.other]].id);
        j.set("substate", cat_.substates[a.substate[r.other]].id);
        rels.push_back(std::move(j));
    }
    v.set("relations", std::move(rels));
    json::Value known = json::Value::array();
    for (auto e : a.known_events[i]) if (e >= 0) known.push_back(e);
    v.set("known_events", std::move(known));
    if (a.group[i] != kNone) {
        json::Value g = json::Value::array();
        const auto leader = a.group[i];
        for (std::uint32_t j = 0; j < a.size(); ++j)
            if (a.group[j] == leader && j != i) g.push_back(j);
        v.set("group", std::move(g));
        v.set("group_leader", leader);
    }
    if (a.partner[i] != kNone) { v.set("partner", a.partner[i]); v.set("partner_name", resident_name(a.partner[i])); }
    const auto& mv = a.move[i];
    if (mv.active) {
        json::Value trip = json::Value::object();
        trip.set("target", building(mv.target_building));
        trip.set("pending", cat_.activities[a.pending_activity[i]].id);
        json::Value legs = json::Value::array();
        static const char* leg_names[] = {"walk", "bike", "drive", "wait", "ride"};
        for (int k = 0; k < mv.leg_count; ++k) legs.push_back(leg_names[static_cast<int>(mv.legs[static_cast<std::size_t>(k)].kind)]);
        trip.set("legs", std::move(legs));
        trip.set("leg", mv.leg);
        // Remaining path for drawing.
        json::Value path = json::Value::array();
        for (int k = mv.leg; k < mv.leg_count; ++k) {
            const auto& leg = mv.legs[static_cast<std::size_t>(k)];
            if (!leg.route.edges) continue;
            std::uint32_t n = k == mv.leg ? mv.at_node : leg.route.start;
            for (std::size_t e = (k == mv.leg ? mv.edge_index : 0); e < leg.route.size(); ++e) {
                n = s.map.edges[(*leg.route.edges)[e]].other(n);
                json::Value p = json::Value::array();
                p.push_back(round2(s.map.nodes[n].pos.x));
                p.push_back(round2(s.map.nodes[n].pos.y));
                path.push_back(std::move(p));
            }
        }
        trip.set("path", std::move(path));
        v.set("trip", std::move(trip));
    }
    v.set("explain", behavior::explain(const_cast<SimState&>(s), i));
    return v;
}

} // namespace npc::sim
