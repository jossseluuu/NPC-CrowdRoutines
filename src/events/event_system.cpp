#include "events/event_system.hpp"

#include "behavior/behavior.hpp"
#include "behavior/movement.hpp"

#include <algorithm>
#include <cmath>

namespace npc::events {

using pop::kNone;

namespace {

constexpr std::size_t kMaxHistory = 400;

const data::EventType& type_of(const sim::SimState& s, const sim::EventInstance& e) {
    return s.cat.events[static_cast<std::size_t>(e.type)];
}

Vec2 clamp_to_map(const sim::SimState& s, Vec2 p) {
    return {std::clamp(p.x, 5.f, s.map.width - 5.f), std::clamp(p.y, 5.f, s.map.height - 5.f)};
}

/// Recomputes weather and per-edge speed factors from all active events.
void recompute_effects(sim::SimState& s) {
    std::fill(s.edge_speed_factor.begin(), s.edge_speed_factor.end(), 1.f);
    std::fill(s.walk_factor_edge.begin(), s.walk_factor_edge.end(), 1.f);
    std::fill(s.attraction.begin(), s.attraction.end(), 0.f);
    s.weather = {};
    float weather_severity = -1;
    for (const auto& e : s.events) {
        if (e.phase != sim::EventPhase::Active) continue;
        const auto& t = type_of(s, e);
        if (t.location == data::EventLocation::Citywide) {
            if (t.effects.weather >= 0 && e.severity > weather_severity) {
                weather_severity = e.severity;
                s.weather.kind = static_cast<data::Weather>(t.effects.weather);
                s.weather.walk = 1.f - (1.f - t.effects.walk_speed) * e.severity;
                s.weather.car = 1.f - (1.f - t.effects.car_speed) * e.severity;
                s.weather.outdoor_penalty = t.effects.outdoor_penalty * e.severity;
            }
            continue;
        }
        const float r2 = e.radius * e.radius;
        for (std::uint32_t k = 0; k < s.map.edges.size(); ++k) {
            const auto& edge = s.map.edges[k];
            const Vec2 mid = (s.map.nodes[edge.a].pos + s.map.nodes[edge.b].pos) * 0.5f;
            if (length_sq(mid - e.pos) > r2) continue;
            s.edge_speed_factor[k] *= t.effects.car_speed;
            s.walk_factor_edge[k] *= t.effects.walk_speed;
        }
        if (t.effects.crowd[1] > 0 || t.effects.attract > 0) {
            for (const auto& b : s.map.buildings) {
                if (length_sq(b.center - e.pos) > r2) continue;
                const bool open_space = b.floors == 0;
                if (e.building == b.id || (open_space && t.effects.crowd[1] > 0))
                    s.attraction[b.id] = std::max(s.attraction[b.id], e.severity * (t.effects.crowd[1] > 0 ? 1.f : 0.3f));
            }
        }
    }
}

void apply_start(sim::SimState& s, sim::EventInstance& e) {
    const auto& t = type_of(s, e);
    if (t.location == data::EventLocation::Citywide) return;
    const float r2 = e.radius * e.radius;
    if (t.effects.block_roads || t.effects.evacuate) {
        for (std::uint32_t k = 0; k < s.map.edges.size(); ++k) {
            const auto& edge = s.map.edges[k];
            const Vec2 mid = (s.map.nodes[edge.a].pos + s.map.nodes[edge.b].pos) * 0.5f;
            if (length_sq(mid - e.pos) > r2 * (t.effects.block_roads ? 0.55f : 1.f)) continue;
            if (t.effects.block_roads) s.router->set_car_blocked(k, true);
            if (t.effects.evacuate) s.router->set_hazard(k, 900.f * e.severity);
            e.blocked_edges.push_back(k);
        }
    }
    if (t.effects.close_buildings) {
        for (const auto& b : s.map.buildings) {
            if (length_sq(b.center - e.pos) > r2) continue;
            if (b.category == s.cat.home_category && !t.effects.evacuate) continue; // outages don't close homes
            if (s.closed[b.id] < 255) ++s.closed[b.id];
            e.closed_buildings.push_back(b.id);
        }
    }
    if (t.effects.bus_delay) {
        float best = 1e30f;
        std::uint32_t pick = kNone;
        for (std::uint32_t b = 0; b < s.buses.size(); ++b) {
            const float d = length_sq(s.buses[b].pos - e.pos);
            if (d < best) { best = d; pick = b; }
        }
        if (pick != kNone) s.buses[pick].delay = static_cast<float>(e.end - e.start);
    }
    e.crowd_target = t.effects.crowd[1] > 0
                         ? static_cast<std::uint32_t>(t.effects.crowd[0] + (t.effects.crowd[1] - t.effects.crowd[0]) * e.severity) : 0;
}

void apply_end(sim::SimState& s, sim::EventInstance& e) {
    // Re-derive closures from the events that remain active (several events may overlap).
    for (auto k : e.blocked_edges) {
        bool still_blocked = false, still_hazard = false;
        for (const auto& o : s.events) {
            if (o.id == e.id || o.phase != sim::EventPhase::Active) continue;
            if (std::find(o.blocked_edges.begin(), o.blocked_edges.end(), k) == o.blocked_edges.end()) continue;
            still_blocked |= type_of(s, o).effects.block_roads;
            still_hazard |= type_of(s, o).effects.evacuate;
        }
        if (!still_blocked) s.router->set_car_blocked(k, false);
        if (!still_hazard) s.router->set_hazard(k, 0.f);
    }
    for (auto b : e.closed_buildings)
        if (s.closed[b] > 0) --s.closed[b];
    e.blocked_edges.clear();
    e.closed_buildings.clear();
}

/// Ends the emergency reaction of resident i and lets it plan normally again.
void end_reaction(sim::SimState& s, std::uint32_t i) {
    auto& a = s.agents;
    a.reacting_to[i] = -1;
    a.partner[i] = kNone;
    if (a.move[i].active) movement::cancel_trip(s, i);
    behavior::set_state(s, i, s.cat.act_travelling, s.cat.sub("walking"));
    a.commit_until[i] = s.now();
    a.next_decision[i] = s.now();
}

void start_reaction(sim::SimState& s, std::uint32_t i, sim::EventInstance& e, int substate, Vec2 target, std::uint32_t building,
                    double minutes) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    if (a.move[i].active) movement::cancel_trip(s, i);
    a.group[i] = kNone;
    a.reacting_to[i] = e.id;
    behavior::set_state(s, i, cat.act_emergency, substate);
    a.commit_until[i] = s.now() + minutes * 60.0;
    a.next_decision[i] = a.commit_until[i];
    ++e.reacted;
    if (building == kNone && distance(target, a.location[i] != kNone ? s.map.buildings[a.location[i]].center : a.pos[i]) < 4.f) {
        // Already in place (e.g. helping right where it happened).
        if (a.location[i] != kNone) { a.pos[i] = s.map.buildings[a.location[i]].center; }
        return;
    }
    if (!movement::start_trip(s, i, building, clamp_to_map(s, target), world::TravelMode::Walk)) {
        a.outdoor[i] = 1;
    }
}

/// Decides how resident i reacts to event e that it just learned about.
void react(sim::SimState& s, std::uint32_t i, sim::EventInstance& e) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const auto& t = type_of(s, e);
    const auto& tr = a.traits[i];
    const Vec2 here = a.location[i] != kNone ? s.map.buildings[a.location[i]].center : a.pos[i];
    const float d = distance(here, e.pos);
    const bool citywide = t.location == data::EventLocation::Citywide;
    const bool inside = !citywide && d < e.radius;
    const bool outdoors = a.location[i] == kNone || a.outdoor[i];
    auto& need = a.needs[i];
    need[data::Fear] = std::min(1.f, need[data::Fear] + e.severity * (inside ? 0.35f : 0.1f) * (0.4f + tr[data::Neuroticism]));
    need[data::Stress] = std::min(1.f, need[data::Stress] + e.severity * (inside ? 0.15f : 0.05f));

    if (a.activity[i] == cat.act_emergency) return;                        // already reacting to something
    const auto& mv = a.move[i];
    const bool in_vehicle = mv.active && (mv.legs[mv.leg].kind == pop::LegKind::Drive || mv.legs[mv.leg].kind == pop::LegKind::Ride);
    const bool asleep = a.activity[i] == cat.act_sleeping;

    struct Option { int substate; float w; };
    std::vector<Option> opts;
    const auto& r = t.response;
    const float near_factor = citywide ? 1.f : std::clamp(1.5f - d / (e.radius * 3.f), 0.f, 1.5f);
    if (!asleep && !in_vehicle) {
        opts.push_back({cat.sub("watching"), r.watch * (0.3f + tr[data::Curiosity]) * (1.f - 0.6f * tr[data::RiskAversion]) * near_factor * (citywide ? 0.f : 1.f)});
        opts.push_back({cat.sub("investigating"), r.investigate * tr[data::Curiosity] * (1.f - tr[data::RiskAversion]) * near_factor * (citywide ? 0.f : 1.f)});
        opts.push_back({cat.sub("helping"), r.help * tr[data::Empathy] * (1.f - 0.5f * tr[data::RiskAversion]) * (d < e.radius * 2.f ? 1.f : 0.f)});
        opts.push_back({cat.sub("fleeing"), r.flee * (0.3f + need[data::Fear] + 0.5f * tr[data::Neuroticism] + 0.5f * tr[data::RiskAversion]) * (d < e.radius * 1.6f ? 1.f : 0.f)});
        opts.push_back({cat.sub("sheltering"), r.shelter * (outdoors ? 1.f : 0.f) * (0.4f + tr[data::RiskAversion])});
    }
    if (!in_vehicle) opts.push_back({cat.sub("evacuating"), r.evacuate * (inside ? 2.5f : 0.f) * (asleep ? 0.8f : 1.f)});
    float ignore = (1.2f - 0.5f * tr[data::Curiosity]) + (d > e.radius * 3.f ? 2.5f : 0.f) + (asleep ? 3.f : 0.f) + (in_vehicle ? 2.f : 0.f);
    for (const auto& it : a.agenda[i])
        if (it.hard && s.hour() >= it.start && s.hour() < it.end) ignore += 0.8f * tr[data::Conscientiousness];
    std::vector<float> w{ignore};
    for (const auto& o : opts) w.push_back(std::max(0.f, o.w));
    const auto pick = s.rng_events.weighted(w);
    if (pick == 0) return;
    const int sub = opts[pick - 1].substate;
    const double remaining = std::max(5.0, (e.end - s.now()) / 60.0);
    const Vec2 away = normalized(here - e.pos + Vec2{0.1f, 0.05f});
    pop::ActionKind kind = pop::ActionKind::Watched;
    if (sub == cat.sub("watching")) {
        start_reaction(s, i, e, sub, e.pos + away * (e.radius * s.rng_events.range(1.05f, 1.4f)), kNone, std::min(remaining, 30.0));
    } else if (sub == cat.sub("investigating")) {
        start_reaction(s, i, e, sub, e.pos + away * (e.radius * 0.5f), kNone, std::min(remaining, 20.0));
    } else if (sub == cat.sub("helping")) {
        kind = pop::ActionKind::Helped;
        start_reaction(s, i, e, sub, e.pos + away * 3.f, kNone, std::min(remaining, 25.0));
    } else if (sub == cat.sub("fleeing")) {
        kind = pop::ActionKind::Fled;
        start_reaction(s, i, e, sub, e.pos + away * (e.radius * 2.6f + 30.f), kNone, std::min(remaining + 5.0, 25.0));
    } else if (sub == cat.sub("evacuating")) {
        kind = pop::ActionKind::Evacuated;
        start_reaction(s, i, e, sub, e.pos + away * (e.radius * 1.5f + 25.f), kNone, remaining + 10.0);
    } else if (sub == cat.sub("sheltering")) {
        kind = pop::ActionKind::Sheltered;
        // Nearest open roofed building that is not closed.
        std::uint32_t best = kNone;
        float bd = 1e30f;
        for (const auto& b : s.map.buildings) {
            if (b.floors == 0 || !s.building_open(b.id)) continue;
            const float dd = length_sq(b.center - here);
            if (dd < bd) { bd = dd; best = b.id; }
        }
        if (best == a.location[i]) return; // already sheltered
        start_reaction(s, i, e, sub, {}, best, remaining);
    }
    a.log(i, s.now(), kind, static_cast<std::uint32_t>(e.id));
}

/// Picks the participants of a conflict and starts the chase.
void start_conflict(sim::SimState& s, sim::EventInstance& e, std::vector<std::uint32_t> involved) {
    auto& a = s.agents;
    if (involved.size() < 2) {
        involved.clear();
        std::vector<std::pair<float, std::uint32_t>> near;
        for (std::uint32_t i = 0; i < a.size(); ++i) {
            if (!a.outdoor[i] || a.activity[i] == s.cat.act_emergency || a.age[i] < 14) continue;
            const float d = length_sq(a.pos[i] - e.pos);
            if (d < e.radius * e.radius * 4.f) near.emplace_back(d, i);
        }
        std::sort(near.begin(), near.end());
        for (std::size_t k = 0; k < near.size() && involved.size() < 2; ++k) involved.push_back(near[k].second);
    }
    if (involved.size() < 2) return;
    const auto x = involved[0], y = involved[1];
    const bool x_aggr = a.traits[x][data::Aggression] >= a.traits[y][data::Aggression];
    const auto pursuer = x_aggr ? x : y, runner = x_aggr ? y : x;
    const Vec2 away = normalized(a.pos[runner] - e.pos + Vec2{0.2f, 0.1f});
    const Vec2 escape = e.pos + away * (e.radius * 3.f + 40.f);
    const double minutes = std::max(3.0, (e.end - s.now()) / 60.0);
    start_reaction(s, runner, e, s.cat.sub("fleeing"), escape, kNone, minutes);
    start_reaction(s, pursuer, e, s.cat.sub("pursuing"), escape + away * -4.f, kNone, minutes);
    a.partner[pursuer] = runner;
    a.partner[runner] = pursuer;
    a.log(pursuer, s.now(), pop::ActionKind::Pursued, runner);
    a.log(runner, s.now(), pop::ActionKind::Fled, static_cast<std::uint32_t>(e.id));
    a.learn(pursuer, e.id);
    a.learn(runner, e.id);
}

Vec2 pick_location(sim::SimState& s, const data::EventType& t, std::uint32_t& building, int& district) {
    auto& rng = s.rng_events;
    const auto& map = s.map;
    building = kNone;
    district = -1;
    switch (t.location) {
        case data::EventLocation::Road: {
            for (int tries = 0; tries < 50; ++tries) {
                const auto& e = map.edges[rng.below(static_cast<std::uint32_t>(map.edges.size()))];
                if (e.cars()) return lerp(map.nodes[e.a].pos, map.nodes[e.b].pos, rng.range(0.3f, 0.7f));
            }
            break;
        }
        case data::EventLocation::Building: {
            std::vector<std::uint32_t> pool;
            for (int c : t.categories) {
                const auto& list = map.buildings_by_category[static_cast<std::size_t>(c)];
                pool.insert(pool.end(), list.begin(), list.end());
            }
            if (pool.empty()) for (const auto& b : map.buildings) pool.push_back(b.id);
            building = pool[rng.below(static_cast<std::uint32_t>(pool.size()))];
            return map.buildings[building].center;
        }
        case data::EventLocation::District: {
            std::vector<int> pool;
            for (std::size_t d = 0; d < map.districts.size(); ++d)
                if (t.districts.empty() || std::find(t.districts.begin(), t.districts.end(), map.districts[d].kind) != t.districts.end())
                    pool.push_back(static_cast<int>(d));
            if (pool.empty()) for (std::size_t d = 0; d < map.districts.size(); ++d) pool.push_back(static_cast<int>(d));
            district = pool[rng.below(static_cast<std::uint32_t>(pool.size()))];
            return map.districts[static_cast<std::size_t>(district)].center;
        }
        case data::EventLocation::Citywide: return {map.width / 2, map.height / 2};
        case data::EventLocation::Anywhere: break;
    }
    // Anywhere: near a random populated spot (a random resident outdoors if possible).
    for (int tries = 0; tries < 30; ++tries) {
        const auto i = rng.below(static_cast<std::uint32_t>(std::max<std::size_t>(1, s.agents.size())));
        if (i < s.agents.size() && s.agents.outdoor[i]) return s.agents.pos[i];
    }
    return map.nodes[rng.below(static_cast<std::uint32_t>(map.nodes.size()))].pos;
}

} // namespace

// ---------------------------------------------------------------------------------------------
std::int32_t spawn(sim::SimState& s, int type, Vec2 pos, bool manual, std::vector<std::uint32_t> involved, float radius,
                   float duration_minutes, float severity) {
    if (type < 0 || type >= static_cast<int>(s.cat.events.size())) return -1;
    const auto& t = s.cat.events[static_cast<std::size_t>(type)];
    auto& rng = s.rng_events;
    sim::EventInstance e;
    e.id = s.next_event_id++;
    e.type = type;
    e.manual = manual;
    if (t.location == data::EventLocation::Citywide) {
        // Only one weather system at a time: a new one replaces the previous.
        for (auto& o : s.events)
            if (o.phase == sim::EventPhase::Active && s.cat.events[static_cast<std::size_t>(o.type)].location == data::EventLocation::Citywide) {
                o.end = s.now();
            }
    }
    if (pos.x < 0) pos = pick_location(s, t, e.building, e.district);
    else if (t.location == data::EventLocation::Building) {
        float bd = 1e30f;
        for (const auto& b : s.map.buildings) {
            const float d = length_sq(b.center - pos);
            if (d < bd) { bd = d; e.building = b.id; }
        }
    }
    e.pos = clamp_to_map(s, pos);
    e.radius = radius >= 0 ? radius : rng.range(t.radius[0], t.radius[1]);
    e.severity = severity >= 0 ? std::clamp(severity, 0.f, 1.f) : rng.range(t.severity[0], t.severity[1]);
    const float minutes = duration_minutes > 0 ? duration_minutes : rng.range(t.duration[0], t.duration[1]);
    e.start = s.now();
    e.end = e.start + minutes * 60.0;
    e.recovery_end = e.end + t.recovery * 60.0;
    apply_start(s, e);
    s.events.push_back(e);
    recompute_effects(s);
    if (t.effects.conflict) start_conflict(s, s.events.back(), std::move(involved));
    return e.id;
}

void end_event(sim::SimState& s, std::int32_t id) {
    for (auto& e : s.events)
        if (e.id == id && e.phase == sim::EventPhase::Active) e.end = s.now();
}

void update(sim::SimState& s, float dt) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const double now = s.now();
    const float dh = dt / 3600.f;

    // 1. Random generation (Poisson process per type).
    if (s.config.random_events && s.config.chaos > 0) {
        std::size_t active = 0;
        for (const auto& e : s.events) active += e.phase == sim::EventPhase::Active;
        const std::size_t cap = 6 + static_cast<std::size_t>(4 * s.config.chaos);
        for (const auto& t : cat.events) {
            if (active >= cap) break;
            double rate = t.base_rate / 86400.0 * s.config.chaos * t.hour_multiplier(s.hour()) * (s.clock.is_weekend() ? t.weekend : 1.f);
            // Population scaling: more residents, more incidents caused by people.
            if (t.category == "social" || t.category == "hazard")
                rate *= std::clamp(static_cast<double>(a.size()) / 5000.0, 0.3, 4.0);
            if (s.rng_events.uniform() < rate * dt) { spawn(s, t.index, {-1, -1}, false); ++active; }
        }
    }

    // 2. Lifecycle.
    bool changed = false;
    for (auto& e : s.events) {
        if (e.phase == sim::EventPhase::Active && now >= e.end) {
            e.phase = sim::EventPhase::Recovering;
            apply_end(s, e);
            changed = true;
        }
        if (e.phase == sim::EventPhase::Recovering && now >= e.recovery_end) e.phase = sim::EventPhase::Ended;
    }
    for (auto& e : s.events)
        if (e.phase == sim::EventPhase::Ended) {
            s.event_history.push_back(e);
            if (s.event_history.size() > kMaxHistory) s.event_history.erase(s.event_history.begin());
        }
    std::erase_if(s.events, [](const sim::EventInstance& e) { return e.phase == sim::EventPhase::Ended; });
    if (changed) recompute_effects(s);

    // 3. Area effects on needs.
    for (const auto& e : s.events) {
        if (e.phase != sim::EventPhase::Active) continue;
        const auto& t = type_of(s, e);
        const bool citywide = t.location == data::EventLocation::Citywide;
        const float r2 = e.radius * e.radius;
        for (std::uint32_t i = 0; i < a.size(); ++i) {
            if (citywide) { if (!a.outdoor[i]) continue; }
            else if (length_sq(a.pos[i] - e.pos) > r2) continue;
            for (int k = 0; k < data::kNeedCount; ++k) {
                const auto kk = static_cast<std::size_t>(k);
                if (t.effects.needs[kk] != 0.f)
                    a.needs[i][kk] = std::clamp(a.needs[i][kk] + t.effects.needs[kk] * e.severity * dh, 0.f, 1.f);
            }
        }
    }

    // 4. Perception, public alerts and word of mouth (every 5 s of simulated time).
    if (s.clock.tick % 5 == 0) {
        for (auto& e : s.events) {
            if (e.phase != sim::EventPhase::Active) continue;
            const auto& t = type_of(s, e);
            const bool citywide = t.location == data::EventLocation::Citywide;
            const bool alert = t.perception.alert_minutes >= 0 && now >= e.start + t.perception.alert_minutes * 60.0;
            const float see2 = std::pow(e.radius * t.perception.see, 2.f), hear2 = std::pow(e.radius * t.perception.hear, 2.f);
            const float alert2 = std::pow(e.radius * 3.f + 150.f, 2.f);
            for (std::uint32_t i = 0; i < a.size(); ++i) {
                if (a.knows(i, e.id)) continue;
                bool learns = false;
                if (citywide) learns = a.outdoor[i] || alert || s.rng_events.chance(0.02f);
                else {
                    const float d2 = length_sq(a.pos[i] - e.pos);
                    const float attention = 0.4f + 0.6f * a.traits[i][data::Curiosity];
                    if ((a.outdoor[i] && d2 < see2) || d2 < hear2) learns = s.rng_events.chance(0.35f + 0.5f * attention);
                    else if (alert && d2 < alert2) learns = true;
                }
                if (!learns) continue;
                a.learn(i, e.id);
                ++e.perceived;
                a.log(i, now, pop::ActionKind::PerceivedEvent, static_cast<std::uint32_t>(e.id));
                react(s, i, e);
            }
        }
    }
    if (s.clock.tick % 60 == 0) {
        for (auto& e : s.events) {
            if (e.phase != sim::EventPhase::Active) continue;
            const float spread = type_of(s, e).perception.spread / 60.f; // per minute
            if (spread <= 0) continue;
            for (std::uint32_t i = 0; i < a.size(); ++i) {
                if (!a.knows(i, e.id)) continue;
                for (std::size_t k = a.rel_begin(i); k < a.rel_end(i); ++k) {
                    const auto& r = a.rels[k];
                    if (r.kind == pop::RelKind::Rival || a.knows(r.other, e.id) || !s.rng_social.chance(spread * (0.5f + r.bond))) continue;
                    a.learn(r.other, e.id);
                    ++e.perceived;
                    a.log(r.other, now, pop::ActionKind::ToldAbout, i, static_cast<std::uint32_t>(e.id));
                    // Being told rarely triggers running to the scene, but warnings make people inside the area leave.
                    if (length_sq(a.pos[r.other] - e.pos) < e.radius * e.radius) react(s, r.other, e);
                    else a.needs[r.other][data::Stress] = std::min(1.f, a.needs[r.other][data::Stress] + 0.03f * e.severity);
                }
            }
        }
    }

    // 5. Crowd events attract residents with free time (handled by venue choice through s.attraction);
    //    count how many are there for the UI.
    for (auto& e : s.events) {
        if (e.crowd_target == 0) continue;
        std::uint32_t n = 0;
        const float r2 = e.radius * e.radius;
        for (std::uint32_t i = 0; i < a.size(); ++i) n += length_sq(a.pos[i] - e.pos) < r2;
        e.crowd_joined = n;
    }

    // 6. Finish reactions whose time is over or whose event ended.
    if (s.clock.tick % 10 == 0) {
        for (std::uint32_t i = 0; i < a.size(); ++i) {
            if (a.activity[i] != cat.act_emergency) continue;
            const auto* ev = s.find_event(a.reacting_to[i]);
            const bool over = now >= a.commit_until[i] || !ev || ev->phase != sim::EventPhase::Active;
            if (over && !(a.move[i].active && ev && ev->phase == sim::EventPhase::Active)) end_reaction(s, i);
        }
    }
}

json::Value event_json(const sim::SimState& s, const sim::EventInstance& e) {
    json::Value j = json::Value::object();
    const auto& t = s.cat.events[static_cast<std::size_t>(e.type)];
    j.set("id", e.id);
    j.set("type", t.id);
    j.set("x", std::round(e.pos.x));
    j.set("y", std::round(e.pos.y));
    j.set("radius", std::round(e.radius));
    j.set("severity", std::round(e.severity * 100) / 100);
    j.set("start", e.start);
    j.set("end", e.end);
    j.set("start_label", SimClock::format(e.start));
    j.set("phase", e.phase == sim::EventPhase::Active ? "active" : e.phase == sim::EventPhase::Recovering ? "recovering" : "ended");
    j.set("manual", e.manual);
    j.set("citywide", t.location == data::EventLocation::Citywide);
    j.set("perceived", e.perceived);
    j.set("reacted", e.reacted);
    j.set("blocked_edges", static_cast<double>(e.blocked_edges.size()));
    j.set("closed_buildings", static_cast<double>(e.closed_buildings.size()));
    if (e.crowd_target) { j.set("crowd", e.crowd_joined); j.set("crowd_target", e.crowd_target); }
    return j;
}

} // namespace npc::events
