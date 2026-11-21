#pragma once

#include "core/types.hpp"
#include <vector>

namespace npc_crowd::storage {

// Benchmark implementation: components are stored in parallel contiguous arrays.
struct SoaAgentStore {
    void reserve(std::size_t count) {
        position.reserve(count); velocity.reserve(count); home.reserve(count); workplace.reserve(count); destination.reserve(count);
        energy.reserve(count); hunger.reserve(count); stress.reserve(count); social.reserve(count); bladder.reserve(count);
        comfort.reserve(count); health.reserve(count); fatigue.reserve(count); fear.reserve(count); curiosity.reserve(count);
        money.reserve(count); temperature.reserve(count); sociability.reserve(count); risk_aversion.reserve(count);
        patience.reserve(count); conscientiousness.reserve(count); age.reserve(count); job_type.reserve(count); activity.reserve(count);
        wake_hour.reserve(count); sleep_hour.reserve(count); work_start_hour.reserve(count); work_duration.reserve(count);
        breakfast_hour.reserve(count); breakfast_probability.reserve(count); workday_mask.reserve(count);
        breakfast_venue.reserve(count); commute_mode.reserve(count); sex.reserve(count); profile.reserve(count);
    }
    void append(const Agent& a) {
        position.push_back(a.position); velocity.push_back(a.velocity); home.push_back(a.home); workplace.push_back(a.workplace); destination.push_back(a.destination);
        energy.push_back(a.energy); hunger.push_back(a.hunger); stress.push_back(a.stress); social.push_back(a.social); bladder.push_back(a.bladder);
        comfort.push_back(a.comfort); health.push_back(a.health); fatigue.push_back(a.fatigue); fear.push_back(a.fear); curiosity.push_back(a.curiosity);
        money.push_back(a.money); temperature.push_back(a.temperature); sociability.push_back(a.sociability); risk_aversion.push_back(a.risk_aversion);
        patience.push_back(a.patience); conscientiousness.push_back(a.conscientiousness); age.push_back(a.age); job_type.push_back(a.job_type); activity.push_back(a.activity);
        wake_hour.push_back(a.wake_hour); sleep_hour.push_back(a.sleep_hour); work_start_hour.push_back(a.work_start_hour); work_duration.push_back(a.work_duration);
        breakfast_hour.push_back(a.breakfast_hour); breakfast_probability.push_back(a.breakfast_probability); workday_mask.push_back(a.workday_mask);
        breakfast_venue.push_back(a.breakfast_venue); commute_mode.push_back(a.commute_mode);
        sex.push_back(static_cast<std::uint8_t>(a.sex)); profile.push_back(static_cast<std::uint8_t>(a.profile));
    }
    std::size_t size() const { return position.size(); }

    std::vector<Vec2> position, velocity, home, workplace, destination;
    std::vector<float> energy, hunger, stress, social, bladder, comfort, health, fatigue, fear, curiosity, money, temperature;
    std::vector<float> sociability, risk_aversion, patience, conscientiousness;
    std::vector<float> wake_hour, sleep_hour, work_start_hour, work_duration, breakfast_hour, breakfast_probability;
    std::vector<std::uint16_t> age, workday_mask;
    std::vector<std::uint8_t> job_type, breakfast_venue, commute_mode, sex, profile;
    std::vector<Activity> activity;
};

} // namespace npc_crowd::storage
