#include "simulation/simulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
namespace npc_crowd {

Simulation::Simulation(Config config) : config_(config), rng_(config.seed) { initialize(); }

void Simulation::initialize() {
    std::uniform_real_distribution<float> coord(0.f, 500.f), need(0.f, .45f), unit(0.f,1.f);
    std::uniform_real_distribution<float> traits(0.f,1.f);
    std::uniform_int_distribution<int> adult_age(20,62), student_age(16,26), retired_age(64,88);
    if(config_.backend==Backend::Oop) oop_.reserve(config_.population);
    else {
        auto n=config_.population;
        soa_.position.reserve(n); soa_.velocity.reserve(n); soa_.home.reserve(n); soa_.workplace.reserve(n); soa_.destination.reserve(n);
        soa_.energy.reserve(n); soa_.hunger.reserve(n); soa_.stress.reserve(n); soa_.social.reserve(n); soa_.bladder.reserve(n);
        soa_.comfort.reserve(n); soa_.health.reserve(n); soa_.fatigue.reserve(n); soa_.fear.reserve(n); soa_.curiosity.reserve(n);
        soa_.money.reserve(n); soa_.temperature.reserve(n); soa_.sociability.reserve(n); soa_.risk_aversion.reserve(n);
        soa_.patience.reserve(n); soa_.conscientiousness.reserve(n); soa_.age.reserve(n); soa_.job_type.reserve(n); soa_.activity.reserve(n);
        soa_.wake_hour.reserve(n); soa_.sleep_hour.reserve(n); soa_.work_start_hour.reserve(n); soa_.work_duration.reserve(n);
        soa_.breakfast_hour.reserve(n); soa_.breakfast_probability.reserve(n); soa_.workday_mask.reserve(n);
        soa_.breakfast_venue.reserve(n); soa_.commute_mode.reserve(n); soa_.sex.reserve(n); soa_.profile.reserve(n);
    }
    for(std::size_t i=0;i<config_.population;++i) {
        Agent a; a.id=static_cast<std::uint32_t>(i); a.home={coord(rng_),coord(rng_)}; a.workplace={coord(rng_),coord(rng_)};
        a.position=a.home; a.destination=a.home; a.energy=.75f+need(rng_); a.hunger=need(rng_); a.stress=need(rng_);
        a.social=traits(rng_); a.bladder=traits(rng_)*.4f; a.comfort=traits(rng_); a.health=.65f+traits(rng_)*.35f;
        a.fatigue=traits(rng_)*.3f; a.fear=traits(rng_)*.15f; a.curiosity=traits(rng_); a.money=traits(rng_);
        a.temperature=.5f; a.sociability=traits(rng_); a.risk_aversion=traits(rng_); a.patience=traits(rng_);
        a.conscientiousness=traits(rng_); a.job_type=static_cast<std::uint8_t>(i%5);
        const float cohort=unit(rng_);
        a.profile=cohort<.30f?RoutineProfile::DayWorker:cohort<.44f?RoutineProfile::ShiftWorker:cohort<.59f?RoutineProfile::Student:cohort<.72f?RoutineProfile::Care:cohort<.85f?RoutineProfile::Retired:RoutineProfile::Flexible;
        a.sex=unit(rng_)<.495f?Sex::Male:unit(rng_)<.99f?Sex::Female:Sex::Other;
        a.age=a.profile==RoutineProfile::Student?static_cast<std::uint16_t>(student_age(rng_)):a.profile==RoutineProfile::Retired?static_cast<std::uint16_t>(retired_age(rng_)):static_cast<std::uint16_t>(adult_age(rng_));
        if(a.profile==RoutineProfile::Retired) a.workday_mask=0;
        else {
            a.workday_mask=0;
            const unsigned days=a.profile==RoutineProfile::ShiftWorker?7u:5u;
            const float attendance=a.profile==RoutineProfile::ShiftWorker?.58f:a.profile==RoutineProfile::Care?.68f:a.profile==RoutineProfile::Student?.92f:a.profile==RoutineProfile::Flexible?.78f:.86f;
            for(unsigned day=0;day<days;++day)if(unit(rng_)<attendance)a.workday_mask|=static_cast<std::uint16_t>(1u<<day);
            if(a.workday_mask==0)a.workday_mask=static_cast<std::uint16_t>(1u<<static_cast<unsigned>(unit(rng_)*days));
        }
        if(a.profile==RoutineProfile::ShiftWorker){const int shift=static_cast<int>(unit(rng_)*3);a.work_start_hour=(shift==0?5.f:shift==1?13.f:21.f)+unit(rng_)*.8f;a.work_duration=7.f+unit(rng_)*2.f;}
        else if(a.profile==RoutineProfile::Student){a.work_start_hour=7.5f+unit(rng_)*2.5f;a.work_duration=4.f+unit(rng_)*3.f;}
        else {a.work_start_hour=7.f+unit(rng_)*3.5f;a.work_duration=5.f+unit(rng_)*4.f;}
        a.wake_hour=std::clamp(a.work_start_hour-.6f-unit(rng_)*1.1f,4.f,11.f);
        a.sleep_hour=std::fmod(a.wake_hour+14.5f+unit(rng_)*2.f,24.f);
        a.breakfast_hour=std::fmod(a.wake_hour+.25f+unit(rng_)*1.1f,24.f);
        a.breakfast_probability=unit(rng_)<.72f?.7f:.1f;
        a.breakfast_venue=static_cast<std::uint8_t>(unit(rng_)*4.f);
        a.commute_mode=static_cast<std::uint8_t>(unit(rng_)*5.f);
        if(a.profile==RoutineProfile::Student)a.job_type=static_cast<std::uint8_t>(4+unit(rng_)*2.f);
        else if(a.profile==RoutineProfile::Retired)a.job_type=7;
        else a.job_type=static_cast<std::uint8_t>(unit(rng_)*4.f);
        if(config_.backend==Backend::Oop) oop_.push_back(std::make_unique<Agent>(a));
        else {
            soa_.position.push_back(a.position); soa_.velocity.push_back(a.velocity); soa_.home.push_back(a.home); soa_.workplace.push_back(a.workplace); soa_.destination.push_back(a.destination);
            soa_.energy.push_back(a.energy); soa_.hunger.push_back(a.hunger); soa_.stress.push_back(a.stress); soa_.social.push_back(a.social); soa_.bladder.push_back(a.bladder);
            soa_.comfort.push_back(a.comfort); soa_.health.push_back(a.health); soa_.fatigue.push_back(a.fatigue); soa_.fear.push_back(a.fear); soa_.curiosity.push_back(a.curiosity);
            soa_.money.push_back(a.money); soa_.temperature.push_back(a.temperature); soa_.sociability.push_back(a.sociability); soa_.risk_aversion.push_back(a.risk_aversion);
            soa_.patience.push_back(a.patience); soa_.conscientiousness.push_back(a.conscientiousness); soa_.age.push_back(a.age); soa_.job_type.push_back(a.job_type); soa_.activity.push_back(a.activity);
            soa_.wake_hour.push_back(a.wake_hour); soa_.sleep_hour.push_back(a.sleep_hour); soa_.work_start_hour.push_back(a.work_start_hour); soa_.work_duration.push_back(a.work_duration);
            soa_.breakfast_hour.push_back(a.breakfast_hour); soa_.breakfast_probability.push_back(a.breakfast_probability); soa_.workday_mask.push_back(a.workday_mask);
            soa_.breakfast_venue.push_back(a.breakfast_venue); soa_.commute_mode.push_back(a.commute_mode);
            soa_.sex.push_back(static_cast<std::uint8_t>(a.sex)); soa_.profile.push_back(static_cast<std::uint8_t>(a.profile));
        }
    }
}

static float speed_for_mode(std::uint8_t mode) {
    constexpr float speeds[5]={1.5f,2.2f,3.0f,2.5f,3.5f}; return speeds[mode%5];
}

static Vec2 target_for(Activity act,Vec2 home,Vec2 workplace,Vec2 pos) {
    switch(act) {
        case Activity::Home: return home;
        case Activity::Work: case Activity::Study: case Activity::Care: return workplace;
        case Activity::Commute: return workplace;
        case Activity::Breakfast: return workplace;
        default: return pos;
    }
}

static Vec2 breakfast_target(std::uint8_t venue,Vec2 home,Vec2 workplace) {
    switch(venue) {
        case 0: return home; case 1: return {home.x+50.f,home.y}; case 2: return {home.x-50.f,home.y}; case 3: return workplace;
        default: return home;
    }
}

static void advance(Vec2& pos,Vec2& vel,Vec2 target,float speed,float stress,const std::vector<Event>& events) {
    Vec2 dir=target-pos; float dist=std::sqrt(length_sq(dir));
    if(dist<1.f) return;
    dir*=speed/dist;
    float stress_factor=1.f-stress*0.3f;
    vel=vel*0.8f+dir*0.2f*stress_factor;
    pos+=vel;
    for(const auto& e:events) {
        if(e.type==EventType::TrafficBlock) {
            Vec2 delta=pos-e.position; float d2=length_sq(delta);
            if(d2<e.radius*e.radius && d2>0.0001f) pos+=delta*(0.5f/std::sqrt(d2));
        }
    }
}

Activity Simulation::choose_activity(float energy,float hunger,float stress,float wake,float sleep,float work_start,float work_duration,
                                     float breakfast_time,float breakfast_probability,std::uint16_t workdays) const {
    const float hour=static_cast<float>(world_hour_);
    const int wday=static_cast<int>(week_day_);
    bool work_today=(workdays>>wday)&1u;
    auto in_span=[](float h,float start,float end){ return start<=end ? h>=start && h<end : h>=start || h<end; };
    auto wrap=[](float h){ h=std::fmod(h,24.f); return h<0.f ? h+24.f : h; }; // windows that cross midnight
    if(in_span(hour,sleep,wake) && energy>0.1f) return Activity::Sleep;
    if(breakfast_probability>0.5f && in_span(hour,breakfast_time,wrap(breakfast_time+0.5f))) return Activity::Breakfast;
    if(work_today) {
        float work_end=std::fmod(work_start+work_duration,24.f);
        if(in_span(hour,work_start,work_end)) return Activity::Work;
        float commute_window=1.f;
        if(in_span(hour,wrap(work_start-commute_window),work_start)) return Activity::Commute;
        if(in_span(hour,work_end,wrap(work_end+commute_window))) return Activity::Commute;
    }
    if(hunger>0.7f) return Activity::Eat;
    if(energy<0.3f) return Activity::Home;
    if(stress>0.7f) return Activity::Home; // stressed residents wind down at home instead of going out
    return Activity::Leisure;
}

void Simulation::update_agent(Agent& a) {
    a.hunger=std::min(1.f,a.hunger+.0008f); a.energy=std::max(0.f,a.energy-.00025f);
    a.social=std::min(1.f,a.social+.00012f); a.bladder=std::min(1.f,a.bladder+.0002f);
    a.fatigue=std::min(1.f,a.fatigue+.00015f);
    a.activity=choose_activity(a.energy,a.hunger,a.stress,a.wake_hour,a.sleep_hour,a.work_start_hour,a.work_duration,
                               a.breakfast_hour,a.breakfast_probability,a.workday_mask);
    if(a.activity==Activity::Eat) a.hunger=std::max(0.f,a.hunger-.006f);
    if(a.activity==Activity::Leisure) a.social=std::max(0.f,a.social-.001f);
    if(a.activity==Activity::Home) { a.energy=std::min(1.f,a.energy+.001f); a.fatigue=std::max(0.f,a.fatigue-.0005f); }
    if(a.bladder>.8f) a.stress=std::min(1.f,a.stress+.0005f);
    if(a.activity==Activity::Sleep) a.energy=std::min(1.f,a.energy+.0012f);
    if(a.activity==Activity::Breakfast) a.hunger=std::max(0.f,a.hunger-.004f);
    a.destination=target_for(a.activity,a.home,a.workplace,a.position);
    if(a.activity==Activity::Breakfast)a.destination=breakfast_target(a.breakfast_venue,a.home,a.workplace);
    if(a.activity==Activity::Commute){const float end=std::fmod(a.work_start_hour+a.work_duration,24.f);const float since_end=std::fmod(static_cast<float>(world_hour_)-end+24.f,24.f);if(since_end<.45f)a.destination=a.home;}
    Vec2 separation{};
    grid_.nearby(a.position,3.f,[&](std::uint32_t j,float rr){
        const auto& other=*oop_[j]; if(other.id==a.id) return;
        Vec2 delta=a.position-other.position; auto d2=length_sq(delta);
        if(d2>0.0001f && d2<rr) separation+=delta*(0.15f/std::sqrt(d2));
    });
    a.destination+=separation;
    advance(a.position,a.velocity,a.destination,speed_for_mode(a.commute_mode),a.stress,events_);
    a.stress=std::max(0.f,a.stress-.0004f);
}

void Simulation::update_soa(std::size_t i) {
    auto& energy=soa_.energy[i]; auto& hunger=soa_.hunger[i]; auto& stress=soa_.stress[i];
    hunger=std::min(1.f,hunger+.0008f); energy=std::max(0.f,energy-.00025f);
    soa_.social[i]=std::min(1.f,soa_.social[i]+.00012f); soa_.bladder[i]=std::min(1.f,soa_.bladder[i]+.0002f);
    soa_.fatigue[i]=std::min(1.f,soa_.fatigue[i]+.00015f);
    auto activity=choose_activity(energy,hunger,stress,soa_.wake_hour[i],soa_.sleep_hour[i],soa_.work_start_hour[i],soa_.work_duration[i],soa_.breakfast_hour[i],soa_.breakfast_probability[i],soa_.workday_mask[i]); soa_.activity[i]=activity;
    if(activity==Activity::Eat) hunger=std::max(0.f,hunger-.006f);
    if(activity==Activity::Leisure) soa_.social[i]=std::max(0.f,soa_.social[i]-.001f);
    if(activity==Activity::Sleep) energy=std::min(1.f,energy+.0012f);
    if(activity==Activity::Breakfast) hunger=std::max(0.f,hunger-.004f);
    if(activity==Activity::Home) { energy=std::min(1.f,energy+.001f); soa_.fatigue[i]=std::max(0.f,soa_.fatigue[i]-.0005f); }
    if(soa_.bladder[i]>.8f) stress=std::min(1.f,stress+.0005f);
    auto destination=target_for(activity,soa_.home[i],soa_.workplace[i],soa_.position[i]);
    if(activity==Activity::Breakfast)destination=breakfast_target(soa_.breakfast_venue[i],soa_.home[i],soa_.workplace[i]);
    if(activity==Activity::Commute){const float end=std::fmod(soa_.work_start_hour[i]+soa_.work_duration[i],24.f);const float since_end=std::fmod(static_cast<float>(world_hour_)-end+24.f,24.f);if(since_end<.45f)destination=soa_.home[i];}
    soa_.destination[i]=destination;
    Vec2 separation{};
    grid_.nearby(soa_.position[i],3.f,[&](std::uint32_t j,float rr){
        if(j==i) return; Vec2 delta=soa_.position[i]-soa_.position[j]; auto d2=length_sq(delta);
        if(d2>0.0001f && d2<rr) separation+=delta*(0.15f/std::sqrt(d2));
    });
    destination+=separation;
    advance(soa_.position[i],soa_.velocity[i],destination,speed_for_mode(soa_.commute_mode[i]),stress,events_); stress=std::max(0.f,stress-.0004f);
}

void Simulation::step() {
    ++tick_; const double advanced=world_hour_+config_.speed*.0005;
    if(advanced>=24.0)week_day_=static_cast<std::uint8_t>((week_day_+static_cast<unsigned>(advanced/24.0))%7);
    world_hour_=std::fmod(advanced,24.0); spawn_events();
    std::vector<Vec2> positions; positions.reserve(config_.population);
    if(config_.backend==Backend::Oop) for(const auto& a:oop_) positions.push_back(a->position);
    else positions=soa_.position;
    grid_.rebuild(positions);
    if(config_.backend==Backend::Oop) for(auto& a:oop_) update_agent(*a);
    else for(std::size_t i=0;i<soa_.size();++i) update_soa(i);
}

void Simulation::spawn_events() {
    std::uniform_real_distribution<float> prob(0.f,1.f), coord(50.f,450.f);
    if(prob(rng_)<.001f) events_.push_back({EventType::TrafficBlock,{coord(rng_),coord(rng_)},30.f,static_cast<std::uint32_t>(tick_+500)});
    if(prob(rng_)<.0005f) events_.push_back({EventType::Rain,{coord(rng_),coord(rng_)},100.f,static_cast<std::uint32_t>(tick_+1000)});
    if(prob(rng_)<.0002f) events_.push_back({EventType::Alarm,{coord(rng_),coord(rng_)},80.f,static_cast<std::uint32_t>(tick_+800)});
    if(!events_.empty()) {
        auto it=std::remove_if(events_.begin(),events_.end(),[this](const Event& e){ return tick_>=e.expires; });
        events_.erase(it,events_.end());
    }
}

Metrics Simulation::run() {
    Metrics out; out.tick_ms.reserve(config_.ticks);
    const auto start=std::chrono::steady_clock::now();
    for(std::uint32_t i=0;i<config_.ticks;++i) {
        const auto t0=std::chrono::steady_clock::now(); step();
        const auto t1=std::chrono::steady_clock::now(); out.tick_ms.push_back(std::chrono::duration<double,std::milli>(t1-t0).count());
    }
    out.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); out.events=events_.size();
    out.memory_bytes=static_cast<double>(config_.population)*(config_.backend==Backend::Soa ? sizeof(Vec2)*5+sizeof(float)*22+sizeof(std::uint16_t)*2+sizeof(std::uint8_t)*6 : sizeof(Agent)+sizeof(void*));
    return out;
}

} // namespace npc_crowd
