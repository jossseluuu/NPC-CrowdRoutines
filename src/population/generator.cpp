#include "population/generator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace npc::pop {

namespace {

float wrap24(float h) {
    h = std::fmod(h, 24.f);
    return h < 0 ? h + 24.f : h;
}

world::TravelMode pick_mode(Rng& rng, const std::array<float, 4>& w, int age, bool has_bus) {
    std::array<float, 4> weights = w;
    if (age < 18) weights[3] = 0;            // no driving licence
    if (age < 10) weights[1] *= 0.3f;
    if (!has_bus) weights[2] = 0;
    return static_cast<world::TravelMode>(rng.weighted(weights));
}

std::uint8_t workday_mask(Rng& rng, data::WorkDays days) {
    switch (days) {
        case data::WorkDays::Weekdays: return 0x1F;
        case data::WorkDays::Six: return 0x3F;
        case data::WorkDays::None: return 0;
        case data::WorkDays::Rotating:
        case data::WorkDays::Random: {
            const int count = days == data::WorkDays::Rotating ? 5 : rng.between(3, 5);
            std::array<int, 7> d{0, 1, 2, 3, 4, 5, 6};
            for (int i = 6; i > 0; --i) std::swap(d[static_cast<std::size_t>(i)], d[rng.below(static_cast<std::uint32_t>(i + 1))]);
            std::uint8_t mask = 0;
            for (int i = 0; i < count; ++i) mask |= static_cast<std::uint8_t>(1u << d[static_cast<std::size_t>(i)]);
            return mask;
        }
    }
    return 0x1F;
}

} // namespace

float estimate_travel_hours(const sim::SimState& s, std::uint32_t from, std::uint32_t to, world::TravelMode mode) {
    if (from == kNone || to == kNone || from == to) return 0.f;
    const float d = distance(s.map.buildings[from].center, s.map.buildings[to].center) * 1.3f; // street detour factor
    const float speed = mode == world::TravelMode::Car ? 7.f : mode == world::TravelMode::Bike ? 4.2f
                      : mode == world::TravelMode::Bus ? 3.0f : 1.35f;
    return d / speed / 3600.f + (mode == world::TravelMode::Car || mode == world::TravelMode::Bus ? 0.08f : 0.02f);
}

void generate_population(sim::SimState& s, std::size_t count, std::uint64_t seed) {
    const auto& cat = s.cat;
    auto& a = s.agents;
    auto& map = s.map;
    Rng rng(seed, "population");
    a.resize(count);
    if (count == 0) return;
    const bool has_bus = !map.bus_lines.empty();

    // ---- Homes: shuffle household slots across residential buildings.
    std::vector<std::uint32_t> slots;
    for (const auto& b : map.buildings)
        for (int h = 0; h < b.households; ++h) slots.push_back(b.id);
    for (std::size_t i = slots.size(); i > 1; --i) std::swap(slots[i - 1], slots[rng.below(static_cast<std::uint32_t>(i))]);

    // Profile index groups.
    std::vector<int> adult_profiles, school_profiles;
    for (const auto& p : cat.profiles) {
        if (p.age[1] < 18) school_profiles.push_back(p.index);
        else adult_profiles.push_back(p.index);
    }
    const int retired = cat.profile_index("retired");
    const int uni = cat.profile_index("student_university");
    const int campus = cat.district_index("campus");

    auto sample_profile = [&](const std::vector<int>& group, std::uint32_t home_building, bool elderly_bias) {
        std::vector<float> w;
        for (int p : group) {
            float weight = cat.profiles[static_cast<std::size_t>(p)].weight;
            if (p == uni && campus >= 0 && map.districts[static_cast<std::size_t>(map.buildings[home_building].district)].kind == campus) weight *= 6.f;
            if (elderly_bias && p == retired) weight *= 3.f;
            w.push_back(weight);
        }
        return group[rng.weighted(w)];
    };

    std::size_t idx = 0, slot = 0;
    std::uint32_t household = 0;
    std::vector<std::vector<std::uint32_t>> household_members;
    while (idx < count) {
        const std::uint32_t home = slots.empty() ? 0 : slots[slot % slots.size()];
        ++slot;
        const std::size_t size = std::min<std::size_t>(count - idx, rng.weighted(cat.household_size_weights) + 1);
        household_members.emplace_back();
        const bool elderly_home = rng.chance(0.18f);
        for (std::size_t m = 0; m < size; ++m, ++idx) {
            const auto i = static_cast<std::uint32_t>(idx);
            int profile;
            if (m == 0 || (m == 1 && rng.chance(0.75f)) || school_profiles.empty()) profile = sample_profile(adult_profiles, home, elderly_home);
            else profile = rng.chance(0.55f) ? school_profiles[rng.below(static_cast<std::uint32_t>(school_profiles.size()))]
                                             : sample_profile(adult_profiles, home, elderly_home);
            const auto& p = cat.profiles[static_cast<std::size_t>(profile)];
            a.profile[i] = static_cast<std::uint8_t>(profile);
            a.age[i] = static_cast<std::uint8_t>(rng.between(p.age[0], p.age[1]));
            const float sx = rng.uniformf();
            a.sex[i] = sx < 0.49f ? Sex::Male : sx < 0.985f ? Sex::Female : Sex::Other;
            a.home[i] = home;
            a.household[i] = household;
            household_members.back().push_back(i);

            // Personality: blend of two archetypes plus individual noise.
            std::vector<float> aw;
            for (const auto& ar : cat.archetypes) aw.push_back(ar.profile_affinity[static_cast<std::size_t>(profile)]);
            const auto primary = rng.weighted(aw);
            auto secondary = rng.below(static_cast<std::uint32_t>(cat.archetypes.size()));
            if (secondary == primary) secondary = (secondary + 1) % static_cast<std::uint32_t>(cat.archetypes.size());
            a.archetype[i] = static_cast<std::uint8_t>(primary);
            a.archetype2[i] = static_cast<std::uint8_t>(secondary);
            const auto& t1 = cat.archetypes[primary].traits;
            const auto& t2 = cat.archetypes[secondary].traits;
            for (int t = 0; t < data::kTraitCount; ++t) {
                const auto k = static_cast<std::size_t>(t);
                a.traits[i][k] = rng.normal_clamped(0.7f * t1[k] + 0.3f * t2[k], cat.trait_sd, 0.f, 1.f);
            }
            const auto& commute_w = cat.archetypes[primary].has_commute ? cat.archetypes[primary].commute : p.commute;
            a.commute[i] = pick_mode(rng, commute_w, a.age[i], has_bus);

            // Schedule.
            a.workdays[i] = workday_mask(rng, p.days);
            if (!p.shifts.empty()) a.work_start[i] = p.shifts[rng.below(static_cast<std::uint32_t>(p.shifts.size()))] + rng.range(-0.25f, 0.25f);
            else a.work_start[i] = rng.normal_clamped(p.start_mean, p.start_sd, 0.f, 23.5f);
            a.work_duration[i] = (a.workdays[i] || p.care) ? std::max(0.5f, rng.normal(p.duration_mean, p.duration_sd)) : 0.f;

            // Initial needs: varied so the city does not move in lockstep.
            auto& n = a.needs[i];
            n[data::Energy] = rng.range(0.05f, 0.45f);
            n[data::Hunger] = rng.range(0.1f, 0.5f);
            n[data::Hygiene] = rng.range(0.0f, 0.4f);
            n[data::Social] = rng.range(0.1f, 0.6f);
            n[data::Fun] = rng.range(0.1f, 0.6f);
            n[data::Health] = rng.chance(0.03f) ? rng.range(0.4f, 0.8f) : 0.f;
            n[data::Stress] = rng.range(0.0f, 0.25f) + 0.1f * a.traits[i][data::Neuroticism];
            n[data::Fear] = 0.f;
        }
        ++household;
    }

    // ---- Sleep/wake windows (depend on work and personality).
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto& p = cat.profiles[a.profile[i]];
        const float owl = a.traits[i][data::NightOwl];
        const float sleep_len = rng.normal_clamped(7.6f + (a.age[i] < 18 ? 1.2f : 0.f) - 0.4f * a.traits[i][data::ActivityLevel], 0.6f, 5.5f, 10.5f);
        float wake;
        if (a.work_duration[i] > 0 && a.workdays[i]) {
            const float prep = rng.range(0.6f, 1.4f) + 0.4f; // breakfast, hygiene, commute buffer
            wake = a.work_start[i] - prep;
            if (a.work_start[i] >= 19.f || a.work_start[i] < 4.f) wake = wrap24(a.work_start[i] + a.work_duration[i] + 8.5f); // night shift
        } else {
            wake = rng.normal_clamped(7.6f + 2.2f * (owl - 0.5f) + (p.days == data::WorkDays::None ? 0.6f : 0.f), 0.7f, 5.f, 11.5f);
        }
        a.wake[i] = wrap24(wake);
        a.sleep[i] = wrap24(a.wake[i] - sleep_len);
    }

    // ---- Workplaces and schools.
    std::vector<int> jobs_left(map.buildings.size()), seats_left(map.buildings.size());
    for (const auto& b : map.buildings) {
        jobs_left[b.id] = b.jobs;
        seats_left[b.id] = b.students;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto& p = cat.profiles[a.profile[i]];
        if (p.workplaces.empty()) continue;
        if (std::find(p.workplaces.begin(), p.workplaces.end(), cat.home_category) != p.workplaces.end() &&
            (p.workplaces.size() == 1 || rng.chance(0.55f))) {
            a.work[i] = a.home[i];
            continue;
        }
        const Vec2 home_pos = map.buildings[a.home[i]].center;
        std::uint32_t best = kNone;
        float best_score = -1;
        for (int attempt = 0; attempt < 24; ++attempt) {
            const int cat_idx = p.workplaces[rng.below(static_cast<std::uint32_t>(p.workplaces.size()))];
            if (cat_idx == cat.home_category) continue;
            const auto& list = map.buildings_by_category[static_cast<std::size_t>(cat_idx)];
            if (list.empty()) continue;
            const auto b = list[rng.below(static_cast<std::uint32_t>(list.size()))];
            const int left = p.student ? seats_left[b] : jobs_left[b];
            if (left <= 0) continue;
            const float d = distance(home_pos, map.buildings[b].center);
            const float score = static_cast<float>(std::min(left, 50)) / (1.f + d / (p.student && a.age[i] < 18 ? 500.f : 1200.f));
            if (score > best_score) { best_score = score; best = b; }
        }
        if (best == kNone) {
            // Everything full: overflow into the nearest building of the first category.
            for (int c : p.workplaces) {
                const auto& list = map.buildings_by_category[static_cast<std::size_t>(c)];
                float bd = 1e30f;
                for (auto b : list) {
                    const float d = distance(home_pos, map.buildings[b].center);
                    if (d < bd) { bd = d; best = b; }
                }
                if (best != kNone) break;
            }
        }
        a.work[i] = best;
        if (best != kNone) (p.student ? seats_left[best] : jobs_left[best])--;
        if (best == kNone && !p.student) { a.workdays[i] = 0; a.work_duration[i] = 0; } // no job available: unemployed
    }

    // ---- Social network.
    std::vector<std::vector<Relation>> rel(count);
    auto link = [&](std::uint32_t x, std::uint32_t y, RelKind k, float bond) {
        if (x == y) return;
        for (const auto& r : rel[x]) if (r.other == y) return;
        rel[x].push_back({y, k, bond});
        rel[y].push_back({x, k, bond});
    };
    for (const auto& members : household_members) {
        std::uint32_t first_adult = kNone;
        for (std::size_t m = 0; m < members.size(); ++m)
            for (std::size_t n = m + 1; n < members.size(); ++n) {
                const auto x = members[m], y = members[n];
                const bool both_adults = a.age[x] >= 18 && a.age[y] >= 18;
                const bool partners = both_adults && first_adult == kNone && std::abs(a.age[x] - a.age[y]) < 15;
                if (partners) first_adult = x;
                link(x, y, partners ? RelKind::Partner : RelKind::Family, rng.range(0.7f, 1.f));
            }
    }
    // Neighbours, coworkers and classmates from shared buildings.
    std::vector<std::vector<std::uint32_t>> by_home(map.buildings.size()), by_work(map.buildings.size());
    for (std::uint32_t i = 0; i < count; ++i) {
        by_home[a.home[i]].push_back(i);
        if (a.work[i] != kNone && a.work[i] != a.home[i]) by_work[a.work[i]].push_back(i);
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto& neighbours = by_home[a.home[i]];
        for (int k = 0; k < 2 && neighbours.size() > 1; ++k) {
            const auto j = neighbours[rng.below(static_cast<std::uint32_t>(neighbours.size()))];
            if (a.household[j] != a.household[i] && rng.chance(0.5f + 0.4f * a.traits[i][data::Sociability]))
                link(i, j, RelKind::Neighbour, rng.range(0.2f, 0.6f));
        }
        if (a.work[i] != kNone && a.work[i] != a.home[i]) {
            const auto& mates = by_work[a.work[i]];
            const bool student = cat.profiles[a.profile[i]].student;
            const int links = 1 + static_cast<int>(4 * a.traits[i][data::Sociability]);
            for (int k = 0; k < links && mates.size() > 1; ++k) {
                const auto j = mates[rng.below(static_cast<std::uint32_t>(mates.size()))];
                link(i, j, student ? RelKind::Classmate : RelKind::Coworker, rng.range(0.25f, 0.75f));
            }
        }
    }
    // Friends by age and personality similarity; a few rivals.
    for (std::uint32_t i = 0; i < count; ++i) {
        const int wanted = 1 + static_cast<int>(std::round(5 * a.traits[i][data::Sociability]));
        for (int f = 0; f < wanted; ++f) {
            std::uint32_t best = kNone;
            float best_score = -1e9f;
            for (int c = 0; c < 18; ++c) {
                const auto j = rng.below(static_cast<std::uint32_t>(count));
                if (j == i) continue;
                float sim = 0;
                for (int t = 0; t < data::kTraitCount; ++t) sim -= std::abs(a.traits[i][static_cast<std::size_t>(t)] - a.traits[j][static_cast<std::size_t>(t)]);
                const float age_gap = std::abs(static_cast<float>(a.age[i]) - static_cast<float>(a.age[j])) / 10.f;
                const float dist = distance(map.buildings[a.home[i]].center, map.buildings[a.home[j]].center) / 800.f;
                const float score = sim - age_gap - dist + rng.range(0.f, 0.5f);
                if (score > best_score) { best_score = score; best = j; }
            }
            if (best != kNone) link(i, best, RelKind::Friend, rng.range(0.35f, 0.95f));
        }
        if (rng.chance(0.04f + 0.2f * a.traits[i][data::Aggression])) {
            const auto j = rng.below(static_cast<std::uint32_t>(count));
            if (j != i && !a.relation(i, j)) link(i, j, RelKind::Rival, rng.range(0.f, 0.3f));
        }
    }
    a.rel_offset.assign(count + 1, 0);
    for (std::uint32_t i = 0; i < count; ++i) a.rel_offset[i + 1] = a.rel_offset[i] + static_cast<std::uint32_t>(rel[i].size());
    a.rels.clear();
    a.rels.reserve(a.rel_offset[count]);
    for (auto& r : rel) a.rels.insert(a.rels.end(), r.begin(), r.end());

    // ---- Initial placement: everyone starts at home.
    for (std::uint32_t i = 0; i < count; ++i) {
        a.location[i] = a.home[i];
        a.activity[i] = static_cast<std::uint8_t>(cat.act_home);
        a.substate[i] = static_cast<std::uint8_t>(cat.activities[static_cast<std::size_t>(cat.act_home)].substates.front());
        a.next_decision[i] = s.now() + rng.range(0.f, 600.f); // staggered first decisions
    }
}

void build_agenda(sim::SimState& s, std::uint32_t i, int day) {
    auto& a = s.agents;
    const auto& cat = s.cat;
    const auto& p = cat.profiles[a.profile[i]];
    const int weekday = (s.clock.start_weekday + day) % 7;
    Rng rng(hash_combine(s.config.seed, (static_cast<std::uint64_t>(i) << 20) ^ static_cast<std::uint64_t>(day)), "agenda");
    // Rotating schedules are redrawn every Monday.
    if ((p.days == data::WorkDays::Rotating || p.days == data::WorkDays::Random) && weekday == 0 && day > 0 && a.work[i] != kNone)
        a.workdays[i] = workday_mask(rng, p.days);

    auto& ag = a.agenda[i];
    // Items from yesterday that run past midnight (night shifts, sleep) continue today.
    std::vector<AgendaItem> carry;
    if (a.agenda_day[i] == day - 1)
        for (auto it : ag)
            if (it.end > 24.f) { it.start -= 24.f; it.end -= 24.f; carry.push_back(it); }
    ag = std::move(carry);
    a.agenda_day[i] = day;
    const auto& tr = a.traits[i];
    auto add = [&](float start, float end, int activity, bool hard, std::uint32_t building = kNone) {
        if (activity < 0) return;
        ag.push_back({start, end, static_cast<std::uint8_t>(activity), hard, building});
    };
    const bool works = (a.workdays[i] >> weekday) & 1u;
    // Sleep (from tonight's bedtime to tomorrow's wake-up).
    const float bed = a.sleep[i] + rng.range(-0.3f, 0.4f) + (weekday >= 4 ? 0.5f * tr[data::NightOwl] : 0.f);
    add(bed < 12 ? bed + 24 : bed, (bed < 12 ? bed + 24 : bed) + std::fmod(a.wake[i] - a.sleep[i] + 24.f, 24.f), cat.act_sleeping, false, a.home[i]);
    if (works && a.work_duration[i] > 0) {
        const float jitter = rng.normal(0.f, 0.15f * (1.2f - tr[data::Routine]));
        const float start = a.work_start[i] + jitter;
        if (p.care) {
            // Caregivers: morning school run / care visit and an afternoon one.
            std::uint32_t school = kNone;
            for (std::size_t k = a.rel_begin(i); k < a.rel_end(i); ++k) {
                const auto& r = a.rels[k];
                if ((r.kind == RelKind::Family) && cat.profiles[a.profile[r.other]].student && a.age[r.other] < 14) school = a.work[r.other];
            }
            add(start, start + a.work_duration[i], cat.act_care, true, school);
            add(16.5f + rng.range(0.f, 0.5f), 17.4f + rng.range(0.f, 0.4f), cat.act_care, true, school);
        } else {
            add(start, start + a.work_duration[i], p.student ? cat.act_studying : cat.act_working, true, a.work[i]);
        }
    }
    // Soft plans: they bias decisions but residents may skip them.
    const bool free_day = !works;
    const float evening = 17.5f + rng.range(0.f, 2.5f);
    const int exercise = cat.activity_index("exercise"), shopping = cat.activity_index("shopping"),
              eating_out = cat.activity_index("eating_out"), leisure = cat.activity_index("leisure"),
              errands = cat.activity_index("errands"), health = cat.act_health;
    if (rng.chance(0.15f + 0.7f * tr[data::ActivityLevel] * tr[data::ActivityLevel])) {
        const float t = rng.chance(0.4f) ? a.wake[i] + 0.3f : evening;
        add(t, t + rng.range(0.6f, 1.4f), exercise, false);
    }
    if (rng.chance(0.25f + 0.25f * (1 - tr[data::Thrift]) + (free_day ? 0.15f : 0))) {
        const float t = free_day ? rng.range(10.f, 13.f) : evening + 0.5f;
        add(t, t + rng.range(0.3f, 1.0f), shopping, false);
    }
    if (rng.chance(0.1f + 0.35f * (1 - tr[data::Thrift]) * tr[data::Sociability])) {
        const float t = rng.chance(0.5f) ? 14.f : 21.f;
        add(t, t + rng.range(0.7f, 1.6f), eating_out, false);
    }
    if (rng.chance(0.2f + 0.5f * tr[data::Sociability] + (weekday >= 4 ? 0.2f : 0))) {
        const float t = free_day ? rng.range(12.f, 20.f) : evening + 1.f;
        add(t, t + rng.range(1.f, 2.5f), cat.act_social, false);
    }
    if (free_day && rng.chance(0.3f + 0.4f * tr[data::Curiosity])) {
        const float t = rng.range(11.f, 19.f);
        add(t, t + rng.range(1.f, 3.f), leisure, false);
    }
    if (weekday < 5 && rng.chance(free_day ? 0.15f : 0.05f)) add(rng.range(9.5f, 12.5f), 13.5f, errands, false);
    if (a.needs[i][data::Health] > 0.3f) add(10.f, 11.5f, health, false);
    std::sort(ag.begin(), ag.end(), [](const AgendaItem& x, const AgendaItem& y) { return x.start < y.start; });
}

} // namespace npc::pop
