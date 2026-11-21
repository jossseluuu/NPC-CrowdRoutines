#include "population/agents.hpp"

#include <algorithm>

namespace npc::pop {

const char* rel_kind_name(RelKind k) {
    switch (k) {
        case RelKind::Family: return "family";
        case RelKind::Partner: return "partner";
        case RelKind::Friend: return "friend";
        case RelKind::Coworker: return "coworker";
        case RelKind::Classmate: return "classmate";
        case RelKind::Neighbour: return "neighbour";
        case RelKind::Rival: return "rival";
        case RelKind::kCount: break;
    }
    return "friend";
}

const char* action_kind_name(ActionKind k) {
    static const char* names[] = {"woke", "went_to_sleep", "arrived", "chatted", "argued", "joined_group", "perceived_event",
                                  "told_about", "evacuated", "fled", "helped", "watched", "sheltered", "boarded", "changed_plan",
                                  "pursued"};
    const auto i = static_cast<std::size_t>(k);
    return i < std::size(names) ? names[i] : "action";
}

void Agents::resize(std::size_t n) {
    profile.assign(n, 0); archetype.assign(n, 0); archetype2.assign(n, 0); age.assign(n, 30);
    sex.assign(n, Sex::Other);
    household.assign(n, 0); home.assign(n, kNone); work.assign(n, kNone);
    commute.assign(n, world::TravelMode::Walk);
    traits.assign(n, data::TraitArray{});
    workdays.assign(n, 0);
    work_start.assign(n, 9.f); work_duration.assign(n, 0.f); wake.assign(n, 7.f); sleep.assign(n, 23.f);
    agenda.assign(n, {});
    agenda_day.assign(n, -1);
    rel_offset.assign(n + 1, 0);
    rels.clear();
    needs.assign(n, data::NeedArray{});
    activity.assign(n, 0); substate.assign(n, 0); pending_activity.assign(n, 0);
    since.assign(n, 0); commit_until.assign(n, 0); next_decision.assign(n, 0); next_substate.assign(n, 0); cooldown.assign(n, 0);
    location.assign(n, kNone);
    pos.assign(n, Vec2{});
    outdoor.assign(n, 0);
    move.assign(n, MoveState{});
    group.assign(n, kNone);
    partner.assign(n, kNone);
    distance.assign(n, 0.f);
    speed.assign(n, 0.f);
    std::array<std::int32_t, kKnownEvents> none{};
    none.fill(-1);
    known_events.assign(n, none);
    reacting_to.assign(n, -1);
    history.assign(n * kHistory, HistoryEntry{});
    history_head.assign(n, 0);
    actions.assign(n * kActions, ActionEntry{});
    actions_head.assign(n, 0);
}

const Relation* Agents::relation(std::uint32_t a, std::uint32_t b) const {
    for (std::size_t k = rel_offset[a]; k < rel_offset[a + 1]; ++k)
        if (rels[k].other == b) return &rels[k];
    return nullptr;
}

void Agents::record_state(std::uint32_t i, double t) {
    auto& head = history_head[i];
    history[i * kHistory + head] = {t, activity[i], substate[i], location[i]};
    head = static_cast<std::uint8_t>((head + 1) % kHistory);
}

void Agents::log(std::uint32_t i, double t, ActionKind kind, std::uint32_t a, std::uint32_t b) {
    auto& head = actions_head[i];
    actions[i * kActions + head] = {t, kind, a, b};
    head = static_cast<std::uint8_t>((head + 1) % kActions);
}

bool Agents::knows(std::uint32_t i, std::int32_t event) const {
    const auto& k = known_events[i];
    return std::find(k.begin(), k.end(), event) != k.end();
}

bool Agents::learn(std::uint32_t i, std::int32_t event) {
    auto& k = known_events[i];
    if (std::find(k.begin(), k.end(), event) != k.end()) return false;
    std::rotate(k.rbegin(), k.rbegin() + 1, k.rend()); // shift right, newest first
    k[0] = event;
    return true;
}

} // namespace npc::pop
