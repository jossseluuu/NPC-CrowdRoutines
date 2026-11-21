#include "behavior/behavior.hpp"

#include "behavior/movement.hpp"
#include "events/event_system.hpp"
#include "population/generator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace npc::behavior {

using pop::kNone;
using data::Need;
using data::Trait;

namespace {

constexpr float kNegInf = -1e9f;

float personality_multiplier(const data::TraitArray& mult, const data::TraitArray& traits) {
    float m = 1.f;
    for (int t = 0; t < data::kTraitCount; ++t) {
        const auto k = static_cast<std::size_t>(t);
        if (mult[k] != 0.f) m *= 1.f + mult[k] * (traits[k] - 0.5f) * 2.f;
    }
    return std::clamp(m, 0.15f, 3.f);
}

bool in_windows(const std::vector<data::HourWindow>& ws, double h) {
    if (ws.empty()) return true;
    for (const auto& w : ws)
        if (w.contains(h)) return true;
    return false;
}

/// Agenda item for activity `act` active now (hour h of today's agenda), or nullptr.
const pop::AgendaItem* agenda_now(const pop::Agents& a, std::uint32_t i, int act, double h, bool hard_only) {
    for (const auto& it : a.agenda[i]) {
        if (it.activity != act || (hard_only && !it.hard)) continue;
        if (h >= it.start && h < it.end) return &it;
    }
    return nullptr;
}

bool sleeping_window(const pop::Agents& a, std::uint32_t i, double h) {
    return hour_in_window(h, a.sleep[i], a.wake[i]);
}

std::uint32_t building_capacity(const world::Building& b) {
    return static_cast<std::uint32_t>(std::max(8, b.visitors + b.jobs + b.students + b.households * 4));
}

struct Choice {
    int activity = -1;
    std::uint32_t venue = kNone;
    float score = kNegInf;
    float travel_h = 0;
};

/// Picks the best open venue for an activity. Returns kNone if nothing suitable is open.
std::uint32_t choose_venue(sim::SimState& s, std::uint32_t i, const data::Activity& act, float& travel_h) {
    const auto& a = s.agents;
    const auto& map = s.map;
    const int weekday = s.clock.weekday();
    const double h = s.hour();
    travel_h = 0;
    const Vec2 here = a.location[i] != kNone ? map.buildings[a.location[i]].center : a.pos[i];

    // Someone else's home.
    if (act.friend_home || act.family_home) {
        std::uint32_t best = kNone;
        float best_score = 0;
        for (std::size_t k = a.rel_begin(i); k < a.rel_end(i); ++k) {
            const auto& r = a.rels[k];
            const bool fits = (act.friend_home && r.kind == pop::RelKind::Friend) ||
                              (act.family_home && r.kind == pop::RelKind::Family && a.home[r.other] != a.home[i]);
            if (!fits) continue;
            const auto j = r.other;
            if (a.location[j] != a.home[j] || a.activity[j] == s.cat.act_sleeping) continue; // nobody home / asleep
            const float sc = r.bond / (1.f + distance(here, map.buildings[a.home[j]].center) / 600.f);
            if (sc > best_score) { best_score = sc; best = a.home[j]; }
        }
        if (best != kNone) {
            travel_h = pop::estimate_travel_hours(s, a.location[i] != kNone ? a.location[i] : a.home[i], best, a.commute[i]);
            return best;
        }
        if (act.venues.empty() && !act.own_home) return kNone;
    }

    std::uint32_t best = kNone;
    float best_cost = std::numeric_limits<float>::infinity();
    const float thrift = a.traits[i][data::Thrift], curiosity = a.traits[i][data::Curiosity];
    auto consider = [&](std::uint32_t b) {
        const auto& bd = map.buildings[b];
        if (!s.building_open(b)) return;
        if (!s.cat.building_types[static_cast<std::size_t>(bd.type)].is_open(weekday, h)) return;
        if (s.occupancy[b] >= building_capacity(bd)) return;
        float d = distance(here, bd.center);
        // Favourite places: a stable per-resident preference worth a few hundred metres, larger for the curious.
        const float fav = static_cast<float>(hash_combine(i, b) % 1000) / 1000.f;
        d -= fav * (150.f + 250.f * curiosity);
        d -= s.attraction[b] * 600.f;                              // crowd events pull people
        if (thrift > 0.7f && (bd.type == s.cat.building_index("mall"))) d += 300.f;
        if (d < best_cost) { best_cost = d; best = b; }
    };
    for (int c : act.venues)
        for (auto b : map.buildings_by_category[static_cast<std::size_t>(c)]) consider(b);
    if (best == kNone && act.own_home) best = a.home[i];
    if (best != kNone)
        travel_h = pop::estimate_travel_hours(s, a.location[i] != kNone ? a.location[i] : a.home[i], best,
                                              distance(here, map.buildings[best].center) < 400.f ? world::TravelMode::Walk : a.commute[i]);
    return best;
}

struct Scored {
    float score = kNegInf;
    std::uint32_t venue = kNone;
    float travel_h = 0;
    float needs = 0, schedule = 0, personality = 1, timing = 0, travel = 0;
};

Scored score_activity(sim::SimState& s, std::uint32_t i, const data::Activity& act, bool with_noise) {
    Scored out;
    if (!act.choosable) return out;
    const auto& a = s.agents;
    const auto& cat = s.cat;
    const double h = s.hour();
    const bool weekend = s.clock.is_weekend();
    if (act.weekday_only && weekend) return out;
    const auto& tr = a.traits[i];
    const auto& need = a.needs[i];
    const int ai = act.index;

    // Obligations (work, school, care) only exist inside their agenda block.
    if (act.scheduled) {
        const auto* item = agenda_now(a, i, ai, h, true);
        if (!item) return out;
        out.schedule = 3.f + 1.5f * tr[data::Conscientiousness] + (h > item->start + 0.1 ? 0.6f : 0.f);
        out.venue = item->building;
        if (out.venue == kNone) {
            if (act.location == data::LocationKind::Venues) out.venue = choose_venue(s, i, act, out.travel_h);
            else out.venue = a.work[i] != kNone ? a.work[i] : a.home[i];
        }
        if (out.venue == kNone) return out;
        out.travel_h = pop::estimate_travel_hours(s, a.location[i] != kNone ? a.location[i] : a.home[i], out.venue, a.commute[i]);
        out.score = out.schedule;
        return out;
    }

    float raw = act.base;
    for (int n = 0; n < data::kNeedCount; ++n) {
        const auto k = static_cast<std::size_t>(n);
        if (act.need_weights[k] != 0.f) out.needs += act.need_weights[k] * std::pow(need[k], 1.5f);
    }
    raw += out.needs;
    for (const auto& w : act.hours)
        if (w.contains(h)) out.timing += w.bonus;
    if (weekend) out.timing += act.weekend_bonus;
    raw += out.timing;
    if (agenda_now(a, i, ai, h, false)) { out.schedule = 0.7f + 0.5f * tr[data::Routine]; raw += out.schedule; }

    // Personal sleep window dominates the generic data windows.
    if (ai == cat.act_sleeping) {
        if (sleeping_window(a, i, h)) raw += 2.4f;
        else if (need[data::Energy] < 0.9f) raw -= 2.4f;      // naps only when exhausted
        else raw -= 0.6f;
    }
    if (ai == cat.act_home && sleeping_window(a, i, h)) raw += 0.6f;

    out.personality = personality_multiplier(act.trait_mult, tr) *
                      (0.7f * cat.archetypes[a.archetype[i]].prefs[static_cast<std::size_t>(ai)] +
                       0.3f * cat.archetypes[a.archetype2[i]].prefs[static_cast<std::size_t>(ai)]);
    raw = raw > 0 ? raw * out.personality : raw / out.personality;
    if (act.outdoor) raw -= s.weather.outdoor_penalty * (0.6f + 0.8f * tr[data::RiskAversion]);

    // Where?
    switch (act.location) {
        case data::LocationKind::Home: out.venue = a.home[i]; break;
        case data::LocationKind::Work:
        case data::LocationKind::School: out.venue = a.work[i]; break;
        case data::LocationKind::Venues: out.venue = choose_venue(s, i, act, out.travel_h); break;
        default: out.venue = a.location[i]; break;
    }
    if (out.venue == kNone) return out;
    if (act.location == data::LocationKind::Home && a.location[i] != a.home[i])
        out.travel_h = pop::estimate_travel_hours(s, a.location[i] != kNone ? a.location[i] : a.home[i], a.home[i], a.commute[i]);
    out.travel = out.travel_h * (1.6f - 0.8f * tr[data::Patience]) * 1.8f;
    raw -= out.travel;

    // Hysteresis: keep doing the current thing unless something is clearly better.
    if (a.activity[i] == ai || a.pending_activity[i] == ai) raw += s.now() < a.commit_until[i] ? 1.4f : 0.2f;
    if (with_noise) raw += s.rng_behavior.normal(0.f, 0.08f + 0.14f * (1.f - tr[data::Routine]));
    out.score = raw;
    return out;
}

/// Hours until the next hard agenda obligation starts (or large if none today).
double hours_to_next_obligation(const pop::Agents& a, std::uint32_t i, double h) {
    double best = 1e9;
    for (const auto& it : a.agenda[i])
        if (it.hard && it.start > h) best = std::min(best, static_cast<double>(it.start) - h);
    return best;
}

bool is_free_for_group(const sim::SimState& s, std::uint32_t j, std::uint32_t building) {
    const auto& a = s.agents;
    return a.location[j] == building && a.activity[j] == s.cat.act_home && a.group[j] == kNone && !a.move[j].active &&
           s.now() >= a.commit_until[j] - 600.0;
}

} // namespace

// ---------------------------------------------------------------------------------------------
Vec2 interior_point(const sim::SimState& s, std::uint32_t i, std::uint32_t b) {
    const auto& fp = s.map.buildings[b].footprint;
    const std::uint64_t h = hash_combine(static_cast<std::uint64_t>(i) * 2654435761u, b);
    const float u = 0.15f + 0.7f * static_cast<float>(h & 0xFFFF) / 65535.f;
    const float v = 0.15f + 0.7f * static_cast<float>((h >> 16) & 0xFFFF) / 65535.f;
    if (fp.size() == 4) return lerp(lerp(fp[0], fp[1], u), lerp(fp[3], fp[2], u), v);
    const Vec2 c = polygon_centroid(fp);
    const Vec2 p = fp[static_cast<std::size_t>((h >> 32) % fp.size())];
    return lerp(c, p, 0.6f * u);
}

void set_state(sim::SimState& s, std::uint32_t i, int activity, int substate) {
    auto& a = s.agents;
    if (a.activity[i] == activity && a.substate[i] == substate) return;
    a.activity[i] = static_cast<std::uint8_t>(activity);
    a.substate[i] = static_cast<std::uint8_t>(substate);
    a.since[i] = s.now();
    a.next_substate[i] = s.now() + s.rng_behavior.range(900.f, 2700.f);
    a.record_state(i, s.now());
}

int pick_substate(sim::SimState& s, std::uint32_t i, int activity, int venue_category) {
    const auto& cat = s.cat;
    const auto& act = cat.activities[static_cast<std::size_t>(activity)];
    const auto& a = s.agents;
    const double h = s.hour();
    std::vector<int> candidates;
    std::vector<float> weights;
    auto add_if = [&](bool venue_match) {
        for (int si : act.substates) {
            const auto& sub = cat.substates[static_cast<std::size_t>(si)];
            if (sub.automatic) continue;
            const bool match = venue_category >= 0 && std::find(sub.venues.begin(), sub.venues.end(), venue_category) != sub.venues.end();
            if (venue_match ? !match : !sub.venues.empty()) continue;
            float w = sub.weight * (in_windows(sub.hours, h) ? 1.f : 0.05f);
            for (int n = 0; n < data::kNeedCount; ++n)
                w *= 1.f + sub.needs[static_cast<std::size_t>(n)] * a.needs[i][static_cast<std::size_t>(n)];
            w *= personality_multiplier(sub.traits, a.traits[i]);
            candidates.push_back(si);
            weights.push_back(w);
        }
    };
    add_if(true);
    // Generic substates (no venue) can also happen at a venue, with lower weight, e.g. a coffee break at the office.
    const std::size_t venue_bound = candidates.size();
    add_if(false);
    for (std::size_t k = venue_bound; k < weights.size(); ++k)
        if (venue_bound > 0) weights[k] *= 0.6f;
    if (candidates.empty()) return act.substates.front();
    return candidates[s.rng_behavior.weighted(weights)];
}

// ---------------------------------------------------------------------------------------------
void update_needs(sim::SimState& s, float dt) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const float dh = dt / 3600.f;
    const auto& drift = cat.base_drift;
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        auto& n = a.needs[i];
        const auto& tr = a.traits[i];
        const auto& act = cat.activities[a.activity[i]];
        const float mods[data::kNeedCount] = {
            1.1f - 0.25f * tr[data::ActivityLevel],      // energy
            1.f,                                          // hunger
            1.f,                                          // hygiene
            0.4f + 1.2f * tr[data::Sociability],          // social
            0.6f + 0.8f * tr[data::Curiosity],            // fun
            1.f,                                          // health
            1.f,                                          // stress (negative drift = recovery)
            1.f};                                         // fear
        for (int k = 0; k < data::kNeedCount; ++k) {
            const auto kk = static_cast<std::size_t>(k);
            float rate = drift[kk] * mods[k] + act.effects[kk];
            if (k == data::Stress) rate += 0.025f * tr[data::Neuroticism] * (s.weather.kind != data::Weather::Clear ? 2.f : 1.f);
            if (k == data::Energy && a.activity[i] == cat.act_sleeping && a.substate[i] == cat.sub("restless_sleep")) rate *= 0.6f;
            n[kk] = std::clamp(n[kk] + rate * dh, 0.f, 1.f);
        }
        // Critical needs interrupt flexible activities.
        if ((n[data::Energy] > 0.96f || n[data::Hunger] > 0.93f || n[data::Health] > 0.85f) && !a.move[i].active &&
            !cat.activities[a.activity[i]].scheduled && a.activity[i] != cat.act_emergency && a.next_decision[i] > s.now() + 60)
            a.next_decision[i] = s.now();
    }
}

void refresh_substates(sim::SimState& s) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const double now = s.now();
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        if (now < a.next_substate[i] || a.move[i].active) continue;
        const int act = a.activity[i];
        if (act == cat.act_travelling || act == cat.act_emergency) { a.next_substate[i] = now + 600; continue; }
        if (cat.substates[a.substate[i]].automatic) { a.next_substate[i] = now + 300; continue; }
        const int venue_cat = a.location[i] != kNone ? s.map.buildings[a.location[i]].category : -1;
        const int sub = pick_substate(s, i, act, venue_cat);
        set_state(s, i, act, sub);
    }
}

// ---------------------------------------------------------------------------------------------
void decide(sim::SimState& s, std::uint32_t i) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const double now = s.now(), h = s.hour();
    if (a.agenda_day[i] != s.clock.day()) pop::build_agenda(s, i, s.clock.day());
    if (a.move[i].active || a.activity[i] == cat.act_emergency) { a.next_decision[i] = now + 120; return; }
    if (a.group[i] != kNone && a.group[i] != i) { a.next_decision[i] = now + 120; return; } // followers wait for the leader

    Choice best;
    for (const auto& act : cat.activities) {
        const Scored sc = score_activity(s, i, act, true);
        if (sc.score > best.score) best = {act.index, sc.venue, sc.score, sc.travel_h};
    }
    if (best.activity < 0 || best.venue == kNone) {
        // Nothing possible: go (or stay) home.
        best = {cat.act_home, a.home[i], 0.f, 0.f};
    }
    const auto& act = cat.activities[static_cast<std::size_t>(best.activity)];
    const int prev_activity = a.activity[i];

    // Commitment and next decision time.
    double commit = act.min_minutes * 60.0 * s.rng_behavior.range(0.8f, 1.25f);
    if (act.scheduled) {
        if (const auto* item = agenda_now(a, i, best.activity, h, true)) commit = std::max(300.0, (item->end - h) * 3600.0);
    } else if (best.activity == cat.act_sleeping) {
        commit = std::fmod(a.wake[i] - h + 24.0, 24.0) * 3600.0 + s.rng_behavior.range(-600.f, 900.f);
        commit = std::clamp(commit, 1800.0, 11.0 * 3600.0);
    }
    const double to_obligation = hours_to_next_obligation(a, i, h) * 3600.0;
    if (!act.scheduled && to_obligation < 1e8) {
        const double leave_by = to_obligation - (best.travel_h + 0.25) * 3600.0;
        commit = std::max(300.0, std::min(commit, leave_by));
    }

    if (best.venue == a.location[i]) {
        // Already here: just switch activity.
        if (prev_activity != best.activity) {
            const int venue_cat = s.map.buildings[best.venue].category;
            set_state(s, i, best.activity, pick_substate(s, i, best.activity, venue_cat));
            if (best.activity == cat.act_sleeping) a.log(i, now, pop::ActionKind::WentToSleep);
            if (prev_activity == cat.act_sleeping) a.log(i, now, pop::ActionKind::Woke);
        }
        a.commit_until[i] = now + commit;
        a.next_decision[i] = now + std::max(240.0, commit);
        return;
    }

    // Trip. Short distances are walked; children never drive.
    const Vec2 here = a.location[i] != kNone ? s.map.buildings[a.location[i]].center : a.pos[i];
    const float dist = distance(here, s.map.buildings[best.venue].center);
    world::TravelMode mode = a.commute[i];
    if (dist < 380.f || (mode == world::TravelMode::Bus && dist < 650.f)) mode = world::TravelMode::Walk;
    if (prev_activity == cat.act_sleeping) a.log(i, now, pop::ActionKind::Woke);
    a.pending_activity[i] = static_cast<std::uint8_t>(best.activity);
    if (!movement::start_trip(s, i, best.venue, {}, mode)) {
        a.next_decision[i] = now + 600;
        return;
    }
    a.commit_until[i] = now + commit; // re-based on arrival
    a.next_decision[i] = now + 24 * 3600.0;

    // Company: household members / friends in the same building may come along.
    const std::uint32_t from = a.location[i] != kNone ? a.location[i] : kNone;
    const bool sociable = best.activity == cat.act_social || best.activity == cat.activity_index("leisure") ||
                          best.activity == cat.activity_index("eating_out") || best.activity == cat.activity_index("exercise") ||
                          best.activity == cat.activity_index("shopping");
    if (from != kNone && mode != world::TravelMode::Bus) {
        int companions = 0;
        for (std::size_t k = a.rel_begin(i); k < a.rel_end(i) && companions < 3; ++k) {
            const auto& r = a.rels[k];
            const auto j = r.other;
            if (r.kind == pop::RelKind::Rival || !is_free_for_group(s, j, from)) continue;
            const bool child_to_school = best.activity == cat.act_studying && a.age[i] < 13 && a.age[j] >= 18 &&
                                         (r.kind == pop::RelKind::Family || r.kind == pop::RelKind::Partner);
            if (!child_to_school && !sociable) continue;
            const float p = child_to_school ? 0.85f : r.bond * (0.25f + 0.5f * a.traits[j][data::Sociability]) * a.traits[i][data::Sociability];
            if (!s.rng_social.chance(p)) continue;
            const int join_act = child_to_school && cat.act_care >= 0 ? cat.act_care : best.activity;
            a.pending_activity[j] = static_cast<std::uint8_t>(join_act);
            a.group[i] = i;
            movement::join_trip(s, j, i);
            a.commit_until[j] = now + (child_to_school ? 600.0 : commit);
            a.next_decision[j] = now + 24 * 3600.0;
            a.log(j, now, pop::ActionKind::JoinedGroup, i);
            a.log(i, now, pop::ActionKind::JoinedGroup, j);
            ++companions;
        }
    }
}

void on_arrival(sim::SimState& s, std::uint32_t i) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const double now = s.now();
    const auto& mv = a.move[i];
    const std::uint32_t b = mv.target_building;
    a.group[i] = kNone;
    if (b != kNone) {
        a.location[i] = b;
        a.outdoor[i] = s.map.buildings[b].floors == 0 ? 1 : 0;
        a.pos[i] = interior_point(s, i, b);
        if (s.occupancy[b] < 65535) ++s.occupancy[b];
    } else {
        a.location[i] = kNone;
        a.outdoor[i] = 1;
    }
    a.speed[i] = 0;
    if (a.activity[i] == cat.act_emergency) {
        // Emergency movement finished (reached the safe point / the scene): stay put a while.
        a.next_decision[i] = std::max(a.next_decision[i], now + 300);
        return;
    }
    int act = a.pending_activity[i];
    if (act == cat.act_travelling || act == cat.act_emergency) act = cat.act_home;
    const int venue_cat = b != kNone ? s.map.buildings[b].category : -1;
    int sub = pick_substate(s, i, act, venue_cat);
    if (act == cat.act_care && cat.sub("school_run") >= 0 && venue_cat == cat.category_index("school")) sub = cat.sub("school_run");
    set_state(s, i, act, sub);
    a.log(i, now, pop::ActionKind::Arrived, b);
    const auto& activity = cat.activities[static_cast<std::size_t>(act)];
    double commit = std::max(a.commit_until[i] - now, static_cast<double>(activity.min_minutes) * 60.0);
    if (activity.scheduled) {
        if (const auto* item = agenda_now(a, i, act, s.hour(), true)) commit = std::max(300.0, (item->end - s.hour()) * 3600.0);
    }
    commit = std::min(commit, static_cast<double>(activity.max_minutes) * 60.0);
    a.commit_until[i] = now + commit;
    a.next_decision[i] = now + std::max(300.0, commit);
}

// ---------------------------------------------------------------------------------------------
void encounters(sim::SimState& s) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const double now = s.now();
    // Spatial hash of pedestrians on the street.
    constexpr float cell = 6.f;
    const int gw = static_cast<int>(s.map.width / cell) + 2, gh = static_cast<int>(s.map.height / cell) + 2;
    std::vector<std::uint32_t> head(static_cast<std::size_t>(gw * gh), kNone), next(a.size(), kNone);
    auto walking = [&](std::uint32_t i) {
        const auto& mv = a.move[i];
        return mv.active && mv.legs[mv.leg].kind == pop::LegKind::Walk && a.activity[i] == cat.act_travelling;
    };
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        if (!walking(i)) continue;
        const int cx = std::clamp(static_cast<int>(a.pos[i].x / cell), 0, gw - 1), cy = std::clamp(static_cast<int>(a.pos[i].y / cell), 0, gh - 1);
        auto& h = head[static_cast<std::size_t>(cy * gw + cx)];
        next[i] = h;
        h = i;
    }
    const int chatting = cat.sub("chatting"), arguing = cat.sub("arguing");
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        if (!walking(i) || now < a.cooldown[i] || a.group[i] != kNone) continue;
        const int cx = std::clamp(static_cast<int>(a.pos[i].x / cell), 0, gw - 1), cy = std::clamp(static_cast<int>(a.pos[i].y / cell), 0, gh - 1);
        for (int y = std::max(0, cy - 1); y <= std::min(gh - 1, cy + 1); ++y)
            for (int x = std::max(0, cx - 1); x <= std::min(gw - 1, cx + 1); ++x)
                for (auto j = head[static_cast<std::size_t>(y * gw + x)]; j != kNone; j = next[j]) {
                    if (j <= i || now < a.cooldown[j] || a.group[j] != kNone || !walking(j)) continue;
                    if (length_sq(a.pos[i] - a.pos[j]) > 25.f) continue;
                    const auto* rel = a.relation(i, j);
                    if (!rel) continue;
                    const bool rivals = rel->kind == pop::RelKind::Rival;
                    const float p = rivals ? 0.25f * a.traits[i][data::Aggression] * a.traits[j][data::Aggression]
                                           : 0.18f * rel->bond * (a.traits[i][data::Sociability] + a.traits[j][data::Sociability]);
                    a.cooldown[i] = a.cooldown[j] = now + 1800;
                    if (!s.rng_social.chance(p)) continue;
                    const Vec2 mid = (a.pos[i] + a.pos[j]) * 0.5f;
                    const double minutes = rivals ? s.rng_social.range(2.f, 6.f) : s.rng_social.range(3.f, 12.f) * (0.5f + rel->bond);
                    for (auto [x1, x2] : {std::pair{i, j}, std::pair{j, i}}) {
                        movement::cancel_trip(s, x1);
                        a.partner[x1] = x2;
                        a.pos[x1] = mid + normalized(a.pos[x1] - a.pos[x2] + Vec2{0.01f, 0.02f}) * 0.9f; // face each other
                        set_state(s, x1, cat.act_social, rivals ? arguing : chatting);
                        a.commit_until[x1] = now + minutes * 60.0;
                        a.next_decision[x1] = a.commit_until[x1];
                        a.needs[x1][data::Social] = std::max(0.f, a.needs[x1][data::Social] - (rivals ? 0.f : 0.15f));
                        a.log(x1, now, rivals ? pop::ActionKind::Argued : pop::ActionKind::Chatted, x2);
                    }
                    if (rivals && a.traits[i][data::Aggression] > 0.6f && a.traits[j][data::Aggression] > 0.6f && s.rng_social.chance(0.3f)) {
                        // Escalation: becomes a street fight that others can perceive and react to.
                        events::spawn(s, s.cat.event_index("street_dispute"), mid, false, {i, j});
                    }
                    goto next_resident;
                }
    next_resident:;
    }
}

// ---------------------------------------------------------------------------------------------
json::Value explain(sim::SimState& s, std::uint32_t i) {
    json::Value arr = json::Value::array();
    for (const auto& act : s.cat.activities) {
        if (!act.choosable) continue;
        const Scored sc = score_activity(s, i, act, false);
        json::Value j = json::Value::object();
        j.set("activity", act.id);
        j.set("score", sc.score <= kNegInf / 2 ? json::Value() : json::Value(std::round(sc.score * 100) / 100));
        j.set("needs", std::round(sc.needs * 100) / 100);
        j.set("schedule", std::round(sc.schedule * 100) / 100);
        j.set("timing", std::round(sc.timing * 100) / 100);
        j.set("personality", std::round(sc.personality * 100) / 100);
        j.set("travel", std::round(sc.travel * 100) / 100);
        j.set("venue", sc.venue == kNone ? json::Value() : json::Value(sc.venue));
        arr.push_back(std::move(j));
    }
    return arr;
}

} // namespace npc::behavior
