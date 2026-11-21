#include "data/catalog.hpp"

#include <algorithm>
#include <cmath>

namespace npc::data {

const std::array<const char*, kNeedCount> kNeedNames = {"energy", "hunger", "hygiene", "social", "fun", "health", "stress", "fear"};
const std::array<const char*, kTraitCount> kTraitNames = {"sociability", "conscientiousness", "curiosity", "risk_aversion",
                                                           "patience", "aggression", "empathy", "activity", "thrift",
                                                           "routine", "night_owl", "neuroticism"};
const std::array<const char*, static_cast<int>(Weather::kCount)> kWeatherNames = {"clear", "rain", "storm", "heat"};

bool HourWindow::contains(double h) const {
    return start <= end ? (h >= start && h < end) : (h >= start || h < end);
}

bool BuildingType::is_open(int weekday, double hour) const {
    if (open24h) return true;
    const auto& today = hours[static_cast<std::size_t>(weekday % 7)];
    if (!today.closed()) {
        if (today.open <= today.close ? (hour >= today.open && hour < today.close) : hour >= today.open) return true;
    }
    // Late opening from the previous day (e.g. bar 12:00 -> 01:30).
    const auto& yesterday = hours[static_cast<std::size_t>((weekday + 6) % 7)];
    return !yesterday.closed() && yesterday.open > yesterday.close && hour < yesterday.close;
}

float EventType::hour_multiplier(double hour) const {
    if (hours.empty()) return 1.f;
    for (const auto& w : hours)
        if (w.contains(hour)) return w.bonus;
    return 1.f;
}

namespace {

std::vector<HourWindow> parse_windows(const json::Value& v) {
    std::vector<HourWindow> out;
    for (const auto& w : v.as_array()) {
        HourWindow hw;
        hw.start = w[0].as_float(0);
        hw.end = w[1].as_float(24);
        hw.bonus = w.size() > 2 ? w[2].as_float(1) : 1.f;
        if (hw.end > 24) hw.end -= 24;
        if (hw.start >= 24) hw.start -= 24;
        out.push_back(hw);
    }
    return out;
}

template <class Array, class Names>
bool parse_named(const json::Value& obj, const Names& names, Array& out, const std::string& ctx, std::string& err) {
    for (const auto& [key, val] : obj.as_object()) {
        const auto it = std::find_if(names.begin(), names.end(), [&](const char* n) { return key == n; });
        if (it == names.end()) {
            err = ctx + ": unknown name '" + key + "'";
            return false;
        }
        out[static_cast<std::size_t>(it - names.begin())] = val.as_float(0);
    }
    return true;
}

bool load_json(const std::string& path, json::Value& out, std::string& err) {
    auto r = json::parse_file(path);
    if (!r.ok()) { err = r.error; return false; }
    out = std::move(r.value);
    return true;
}

} // namespace

int Catalog::ensure_category(const std::string& id) {
    const auto it = category_ids_.find(id);
    if (it != category_ids_.end()) return it->second;
    const int idx = static_cast<int>(categories.size());
    categories.push_back(id);
    category_ids_[id] = idx;
    return idx;
}

bool Catalog::load(const std::string& dir, std::string& error) {
    *this = Catalog{};
    data_dir = dir;
    home_category = ensure_category("home");
    json::Value v;
    // Pseudo-categories: someone else's home (friend / family member). Never assigned to building types.
    auto register_pseudo = [this] { ensure_category("home_friend"); ensure_category("home_family"); };
    if (!load_json(dir + "/buildings.json", v, error) || !load_buildings(v, error)) return false;
    register_pseudo();
    if (!load_json(dir + "/activities.json", v, error) || !load_activities(v, error)) return false;
    if (!load_json(dir + "/population.json", v, error) || !load_population(v, error)) return false;
    if (!load_json(dir + "/events.json", v, error) || !load_events(v, error)) return false;

    act_travelling = activity_index("travelling");
    act_emergency = activity_index("emergency");
    act_sleeping = activity_index("sleeping");
    act_home = activity_index("home");
    act_working = activity_index("working");
    act_studying = activity_index("studying");
    act_social = activity_index("social");
    act_care = activity_index("care");
    act_health = activity_index("health");
    for (int a : {act_travelling, act_emergency, act_sleeping, act_home, act_working, act_studying}) {
        if (a < 0) { error = "activities.json: required activities travelling/emergency/sleeping/home/working/studying missing"; return false; }
    }
    for (const char* s : {"walking", "walking_talking", "strolling", "hurrying", "cycling", "waiting_bus", "on_bus", "driving",
                          "stuck_in_traffic", "chatting", "watching", "investigating", "fleeing", "evacuating", "sheltering",
                          "helping", "pursuing", "arguing", "queueing", "night_sleep"}) {
        if (sub(s) < 0) { error = std::string("activities.json: required substate '") + s + "' missing"; return false; }
    }
    return true;
}

bool Catalog::load_buildings(const json::Value& v, std::string& err) {
    const char* day_keys[7] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
    for (const auto& t : v["types"].as_array()) {
        BuildingType b;
        b.id = t.str("id");
        b.color = t.str("color", "#555555");
        b.index = static_cast<int>(building_types.size());
        if (b.id.empty() || building_ids_.count(b.id)) { err = "buildings.json: missing or duplicate type id '" + b.id + "'"; return false; }
        b.category = ensure_category(t.str("category", b.id));
        for (int i = 0; i < 4; ++i) b.size[i] = t["size"][static_cast<std::size_t>(i)].as_float(12);
        for (int i = 0; i < 2; ++i) {
            b.floors[i] = static_cast<int>(t["floors"][static_cast<std::size_t>(i)].as_int(1));
            b.households[i] = static_cast<int>(t["households"][static_cast<std::size_t>(i)].as_int(0));
            b.jobs[i] = static_cast<int>(t["jobs"][static_cast<std::size_t>(i)].as_int(0));
            b.students[i] = static_cast<int>(t["students"][static_cast<std::size_t>(i)].as_int(0));
        }
        b.visitors = static_cast<int>(t.integer("visitors", 0));
        b.open24h = t.boolean("open24h", false) || b.residential();
        const auto& h = t["hours"];
        auto parse_day = [](const json::Value& d) {
            DayHours dh;
            if (d.is_array()) { dh.open = d[0].as_float(9); dh.close = d[1].as_float(18); if (dh.close > 24) dh.close -= 24; }
            return dh;
        };
        for (int d = 0; d < 5; ++d) b.hours[static_cast<std::size_t>(d)] = parse_day(h["weekdays"]);
        if (h.contains("saturday")) b.hours[5] = parse_day(h["saturday"]);
        if (h.contains("sunday")) b.hours[6] = parse_day(h["sunday"]);
        for (int d = 0; d < 7; ++d)
            if (h.contains(day_keys[d])) b.hours[static_cast<std::size_t>(d)] = parse_day(h[day_keys[d]]);
        building_ids_[b.id] = b.index;
        building_types.push_back(std::move(b));
    }
    for (const auto& d : v["districts"].as_array()) {
        DistrictKind k;
        k.id = d.str("id");
        k.color = d.str("color", "#222222");
        k.density = d.flt("density", 0.5f);
        k.index = static_cast<int>(districts.size());
        for (const auto& [type, w] : d["weights"].as_object()) {
            const int bi = building_index(type);
            if (bi < 0) { err = "buildings.json: district '" + k.id + "' uses unknown type '" + type + "'"; return false; }
            k.weights.emplace_back(bi, w.as_float(0));
        }
        district_ids_[k.id] = k.index;
        districts.push_back(std::move(k));
    }
    for (const auto& [type, n] : v["minimums"].as_object()) {
        const int bi = building_index(type);
        if (bi < 0) { err = "buildings.json: minimums uses unknown type '" + type + "'"; return false; }
        minimums.emplace_back(bi, static_cast<int>(n.as_int(0)));
    }
    if (building_types.empty() || districts.empty()) { err = "buildings.json: no types or districts"; return false; }
    return true;
}

bool Catalog::load_activities(const json::Value& v, std::string& err) {
    if (!parse_named(v["base_drift"], kNeedNames, base_drift, "activities.json base_drift", err)) return false;
    for (const auto& a : v["activities"].as_array()) {
        Activity act;
        act.id = a.str("id");
        act.color = a.str("color", "#cccccc");
        act.icon = a.str("icon", "•");
        act.index = static_cast<int>(activities.size());
        if (act.id.empty() || activity_ids_.count(act.id)) { err = "activities.json: missing or duplicate id '" + act.id + "'"; return false; }
        const std::string ctx = "activities.json '" + act.id + "'";
        const auto& loc = a["location"];
        if (loc.is_string()) {
            const std::string l = loc.as_string();
            act.location = l == "home" ? LocationKind::Home : l == "travel" ? LocationKind::Travel : l == "work" ? LocationKind::Work
                         : l == "school" ? LocationKind::School : LocationKind::Anywhere;
        } else {
            act.location = LocationKind::Venues;
            for (const auto& c : loc.as_array()) {
                const std::string name = c.as_string();
                if (name == "home_friend") act.friend_home = true;
                if (name == "home_family") act.family_home = true;
                if (name == "home") { act.own_home = true; continue; }
                const int ci = category_index(name);
                if (ci < 0) { err = ctx + ": unknown venue category '" + name + "'"; return false; }
                act.venues.push_back(ci);
            }
        }
        act.choosable = a.boolean("choosable", false);
        act.scheduled = a.boolean("scheduled", false);
        act.outdoor = a.boolean("outdoor", false);
        act.weekday_only = a.boolean("weekday_only", false);
        act.min_minutes = a.flt("min_minutes", 15);
        act.max_minutes = a.flt("max_minutes", 120);
        act.base = a.flt("base", 0);
        act.weekend_bonus = a.flt("weekend_bonus", 0);
        act.hours = parse_windows(a["hours"]);
        if (!parse_named(a["needs"], kNeedNames, act.need_weights, ctx + " needs", err)) return false;
        if (!parse_named(a["effects"], kNeedNames, act.effects, ctx + " effects", err)) return false;
        if (!parse_named(a["traits"], kTraitNames, act.trait_mult, ctx + " traits", err)) return false;
        for (const auto& s : a["substates"].as_array()) {
            Substate sub;
            sub.id = s.str("id");
            sub.activity = act.index;
            if (sub.id.empty() || substate_ids_.count(sub.id)) { err = ctx + ": missing or duplicate substate '" + sub.id + "'"; return false; }
            sub.weight = s.flt("weight", 1);
            sub.automatic = s.boolean("auto", false);
            for (const auto& h : s["hours"].is_array() && !s["hours"].as_array().empty() && s["hours"][0].is_number()
                                     ? json::Array{s["hours"]} : s["hours"].as_array()) {
                HourWindow hw{h[0].as_float(0), h[1].as_float(24), 1};
                if (hw.end > 24) hw.end -= 24;
                sub.hours.push_back(hw);
            }
            for (const auto& c : s["venue"].as_array()) {
                const int ci = category_index(c.as_string());
                if (ci < 0) { err = ctx + " substate '" + sub.id + "': unknown venue '" + c.as_string() + "'"; return false; }
                sub.venues.push_back(ci);
            }
            if (!parse_named(s["needs"], kNeedNames, sub.needs, ctx + " substate needs", err)) return false;
            if (!parse_named(s["traits"], kTraitNames, sub.traits, ctx + " substate traits", err)) return false;
            const int si = static_cast<int>(substates.size());
            substate_ids_[sub.id] = si;
            act.substates.push_back(si);
            substates.push_back(std::move(sub));
        }
        if (act.substates.empty()) { err = ctx + ": every activity needs at least one substate"; return false; }
        activity_ids_[act.id] = act.index;
        activities.push_back(std::move(act));
    }
    if (activities.size() > 250 || substates.size() > 250) { err = "activities.json: at most 250 activities and 250 substates"; return false; }
    return true;
}

bool Catalog::load_population(const json::Value& v, std::string& err) {
    trait_sd = v.flt("trait_sd", 0.1f);
    for (const auto& w : v["household_size_weights"].as_array()) household_size_weights.push_back(w.as_float(0));
    if (household_size_weights.empty()) household_size_weights = {1.f};
    for (const auto& p : v["profiles"].as_array()) {
        Profile pr;
        pr.id = p.str("id");
        pr.index = static_cast<int>(profiles.size());
        if (pr.id.empty() || profile_ids_.count(pr.id)) { err = "population.json: missing or duplicate profile '" + pr.id + "'"; return false; }
        pr.weight = p.flt("weight", 1);
        pr.age[0] = static_cast<int>(p["age"][0].as_int(18));
        pr.age[1] = static_cast<int>(p["age"][1].as_int(65));
        for (const auto& w : p["workplaces"].as_array()) {
            const int ci = category_index(w.as_string());
            if (ci < 0) { err = "population.json profile '" + pr.id + "': unknown workplace '" + w.as_string() + "'"; return false; }
            pr.workplaces.push_back(ci);
        }
        const std::string days = p.str("days", "weekdays");
        pr.days = days == "six" ? WorkDays::Six : days == "rotating" ? WorkDays::Rotating : days == "random" ? WorkDays::Random
                : days == "none" ? WorkDays::None : WorkDays::Weekdays;
        pr.start_mean = p["start"][0].as_float(9);
        pr.start_sd = p["start"][1].as_float(1);
        pr.duration_mean = p["duration"][0].as_float(8);
        pr.duration_sd = p["duration"][1].as_float(0.5f);
        for (const auto& s : p["shifts"].as_array()) pr.shifts.push_back(s.as_float(8));
        for (std::size_t i = 0; i < 4; ++i) pr.commute[i] = p["commute"][i].as_float(0.25f);
        pr.student = p.boolean("student", false);
        pr.care = p.boolean("care", false);
        profile_ids_[pr.id] = pr.index;
        profiles.push_back(std::move(pr));
    }
    for (const auto& a : v["archetypes"].as_array()) {
        Archetype ar;
        ar.id = a.str("id");
        ar.index = static_cast<int>(archetypes.size());
        if (ar.id.empty() || archetype_ids_.count(ar.id)) { err = "population.json: missing or duplicate archetype '" + ar.id + "'"; return false; }
        if (a["t"].size() != kTraitCount) { err = "population.json archetype '" + ar.id + "': 't' needs 12 trait values"; return false; }
        for (std::size_t i = 0; i < kTraitCount; ++i) ar.traits[i] = a["t"][i].as_float(0.5f);
        ar.profile_affinity.assign(profiles.size(), 1.f);
        for (const auto& [pid, w] : a["profiles"].as_object()) {
            const int pi = profile_index(pid);
            if (pi < 0) { err = "population.json archetype '" + ar.id + "': unknown profile '" + pid + "'"; return false; }
            ar.profile_affinity[static_cast<std::size_t>(pi)] = w.as_float(1);
        }
        ar.prefs.assign(activities.size(), 1.f);
        for (const auto& [aid, m] : a["prefs"].as_object()) {
            const int ai = activity_index(aid);
            if (ai < 0) { err = "population.json archetype '" + ar.id + "': unknown activity '" + aid + "'"; return false; }
            ar.prefs[static_cast<std::size_t>(ai)] = m.as_float(1);
        }
        if (a.contains("commute")) {
            ar.has_commute = true;
            for (std::size_t i = 0; i < 4; ++i) ar.commute[i] = a["commute"][i].as_float(0.25f);
        }
        archetype_ids_[ar.id] = ar.index;
        archetypes.push_back(std::move(ar));
    }
    if (profiles.empty() || archetypes.empty()) { err = "population.json: needs profiles and archetypes"; return false; }
    return true;
}

bool Catalog::load_events(const json::Value& v, std::string& err) {
    for (const auto& e : v["types"].as_array()) {
        EventType ev;
        ev.id = e.str("id");
        ev.index = static_cast<int>(events.size());
        if (ev.id.empty() || event_ids_.count(ev.id)) { err = "events.json: missing or duplicate id '" + ev.id + "'"; return false; }
        const std::string ctx = "events.json '" + ev.id + "'";
        ev.category = e.str("category", "other");
        ev.color = e.str("color", "#ffaa55");
        ev.icon = e.str("icon", "!");
        ev.base_rate = e.flt("base_rate", 0);
        ev.hours = parse_windows(e["hours"]);
        ev.weekend = e.flt("weekend", 1);
        const std::string loc = e.str("location", "anywhere");
        ev.location = loc == "road" ? EventLocation::Road : loc == "building" ? EventLocation::Building
                    : loc == "district" ? EventLocation::District : loc == "citywide" ? EventLocation::Citywide : EventLocation::Anywhere;
        for (const auto& c : e["categories"].as_array()) {
            const int ci = category_index(c.as_string());
            if (ci < 0) { err = ctx + ": unknown category '" + c.as_string() + "'"; return false; }
            ev.categories.push_back(ci);
        }
        for (const auto& d : e["districts"].as_array()) {
            const int di = district_index(d.as_string());
            if (di < 0) { err = ctx + ": unknown district '" + d.as_string() + "'"; return false; }
            ev.districts.push_back(di);
        }
        for (int i = 0; i < 2; ++i) {
            ev.radius[i] = e["radius"][static_cast<std::size_t>(i)].as_float(30);
            ev.duration[i] = e["duration"][static_cast<std::size_t>(i)].as_float(30);
            ev.severity[i] = e["severity"][static_cast<std::size_t>(i)].as_float(0.5f);
        }
        ev.recovery = e.flt("recovery", 0);
        const auto& fx = e["effects"];
        for (int n = 0; n < kNeedCount; ++n) ev.effects.needs[static_cast<std::size_t>(n)] = fx.flt(kNeedNames[static_cast<std::size_t>(n)], 0);
        ev.effects.walk_speed = fx.flt("walk_speed", 1);
        ev.effects.car_speed = fx.flt("car_speed", 1);
        ev.effects.attract = fx.flt("attract", 0);
        ev.effects.outdoor_penalty = fx.flt("outdoor_penalty", 0);
        ev.effects.block_roads = fx.boolean("block_roads", false);
        ev.effects.close_buildings = fx.boolean("close_buildings", false);
        ev.effects.evacuate = fx.boolean("evacuate", false);
        ev.effects.conflict = fx.boolean("conflict", false);
        ev.effects.bus_delay = fx.boolean("bus_delay", false);
        ev.effects.crowd[0] = static_cast<int>(fx["crowd"][0].as_int(0));
        ev.effects.crowd[1] = static_cast<int>(fx["crowd"][1].as_int(0));
        if (fx.contains("weather")) {
            const std::string w = fx.str("weather");
            for (int i = 0; i < static_cast<int>(Weather::kCount); ++i)
                if (w == kWeatherNames[static_cast<std::size_t>(i)]) ev.effects.weather = i;
            if (ev.effects.weather < 0) { err = ctx + ": unknown weather '" + w + "'"; return false; }
        }
        const auto& r = e["response"];
        ev.response = {r.flt("watch", 0), r.flt("investigate", 0), r.flt("flee", 0), r.flt("evacuate", 0), r.flt("shelter", 0), r.flt("help", 0)};
        const auto& p = e["perception"];
        ev.perception = {p.flt("see", 1), p.flt("hear", 0), p.flt("spread", 0), p.contains("alert") ? p.flt("alert", -1) : -1.f};
        event_ids_[ev.id] = ev.index;
        events.push_back(std::move(ev));
    }
    return true;
}

json::Value Catalog::to_json() const {
    json::Value out = json::Value::object();
    json::Value acts = json::Value::array();
    for (const auto& a : activities) {
        json::Value j = json::Value::object();
        j.set("id", a.id);
        j.set("color", a.color);
        j.set("icon", a.icon);
        json::Value subs = json::Value::array();
        for (int s : a.substates) {
            json::Value sj = json::Value::object();
            sj.set("index", s);
            sj.set("id", substates[static_cast<std::size_t>(s)].id);
            subs.push_back(std::move(sj));
        }
        j.set("substates", std::move(subs));
        acts.push_back(std::move(j));
    }
    out.set("activities", std::move(acts));
    auto ids = [](const auto& items) {
        json::Value arr = json::Value::array();
        for (const auto& it : items) arr.push_back(it.id);
        return arr;
    };
    json::Value bts = json::Value::array();
    for (const auto& b : building_types) {
        json::Value j = json::Value::object();
        j.set("id", b.id);
        j.set("category", categories[static_cast<std::size_t>(b.category)]);
        j.set("color", b.color);
        bts.push_back(std::move(j));
    }
    out.set("buildings", std::move(bts));
    json::Value dks = json::Value::array();
    for (const auto& d : districts) {
        json::Value j = json::Value::object();
        j.set("id", d.id);
        j.set("color", d.color);
        dks.push_back(std::move(j));
    }
    out.set("districts", std::move(dks));
    json::Value evs = json::Value::array();
    for (const auto& e : events) {
        json::Value j = json::Value::object();
        j.set("id", e.id);
        j.set("category", e.category);
        j.set("color", e.color);
        j.set("icon", e.icon);
        j.set("citywide", e.location == EventLocation::Citywide);
        evs.push_back(std::move(j));
    }
    out.set("events", std::move(evs));
    out.set("profiles", ids(profiles));
    out.set("archetypes", ids(archetypes));
    json::Value needs = json::Value::array(), traits = json::Value::array(), weather = json::Value::array();
    for (auto n : kNeedNames) needs.push_back(n);
    for (auto t : kTraitNames) traits.push_back(t);
    for (auto w : kWeatherNames) weather.push_back(w);
    out.set("needs", std::move(needs));
    out.set("traits", std::move(traits));
    out.set("weather", std::move(weather));
    return out;
}

} // namespace npc::data
