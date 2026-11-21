#include "server/runner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace npc::server {

namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t kTickWindow = 600;
}

SimRunner::SimRunner(const data::Catalog& cat, const world::MapLibrary& maps) : sim_(cat, maps) { tick_ms_.assign(kTickWindow, 0.f); }

SimRunner::~SimRunner() { stop(); }

void SimRunner::start() {
    if (alive_.exchange(true)) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        publish();
    }
    thread_ = std::thread([this] { loop(); });
}

void SimRunner::stop() {
    if (!alive_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

void SimRunner::set_speed(double s) { speed_ = std::clamp(s, 0.1, 100000.0); }

void SimRunner::step(std::uint64_t ticks) { pending_steps_ += ticks; }

bool SimRunner::rebuild(const std::function<bool(sim::Simulation&, std::string&)>& fn, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool ok = fn(sim_, error);
    std::fill(tick_ms_.begin(), tick_ms_.end(), 0.f);
    ticks_ = 0;
    {
        std::lock_guard<std::mutex> c(cache_mutex_);
        map_ = sim_.map_json().dump();
    }
    publish();
    return ok;
}

void SimRunner::publish() {
    auto frame = sim_.frame_binary();
    auto state = sim_.state_json().dump();
    std::lock_guard<std::mutex> c(cache_mutex_);
    frame_ = std::move(frame);
    state_ = std::move(state);
    if (map_.empty()) map_ = sim_.map_json().dump();
}

std::vector<std::uint8_t> SimRunner::frame() const {
    std::lock_guard<std::mutex> c(cache_mutex_);
    return frame_;
}
std::string SimRunner::state() const {
    std::lock_guard<std::mutex> c(cache_mutex_);
    return state_;
}
std::string SimRunner::map() const {
    std::lock_guard<std::mutex> c(cache_mutex_);
    return map_;
}

RunnerStatus SimRunner::status() const {
    RunnerStatus st;
    st.running = running_;
    st.speed = speed_;
    std::lock_guard<std::mutex> c(cache_mutex_);
    st.achieved = achieved_;
    st.ticks = ticks_;
    std::vector<float> v;
    for (float t : tick_ms_) if (t > 0) v.push_back(t);
    if (!v.empty()) {
        std::sort(v.begin(), v.end());
        auto pct = [&](double p) { return static_cast<double>(v[std::min(v.size() - 1, static_cast<std::size_t>(p * static_cast<double>(v.size() - 1)))]); };
        st.tick_ms_p50 = pct(0.5);
        st.tick_ms_p95 = pct(0.95);
        st.tick_ms_p99 = pct(0.99);
        st.tick_ms_max = v.back();
    }
    return st;
}

void SimRunner::loop() {
    auto last = Clock::now(), last_publish = last, last_measure = last;
    double due = 0, sim_seconds_measure = 0;
    while (alive_) {
        const auto now = Clock::now();
        const double real_dt = std::chrono::duration<double>(now - last).count();
        last = now;
        std::uint64_t to_run = 0;
        double step_seconds = 1.0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            step_seconds = sim_.state().clock.step_seconds;
        }
        if (running_) {
            due += speed_ * real_dt / step_seconds;
            to_run = static_cast<std::uint64_t>(due);
        } else {
            due = 0;
        }
        to_run += pending_steps_.exchange(0);
        // Run ticks within a time budget so the API stays responsive; drop backlog if we cannot keep up.
        const auto budget_end = now + std::chrono::milliseconds(40);
        std::uint64_t ran = 0;
        if (to_run > 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            while (ran < to_run && Clock::now() < budget_end) {
                const auto t0 = Clock::now();
                sim_.step();
                const float ms = std::chrono::duration<float, std::milli>(Clock::now() - t0).count();
                std::lock_guard<std::mutex> c(cache_mutex_);
                tick_ms_[tick_head_] = ms;
                tick_head_ = (tick_head_ + 1) % kTickWindow;
                ++ticks_;
                ++ran;
            }
        }
        if (running_) due = std::min(due - static_cast<double>(std::min(ran, static_cast<std::uint64_t>(due))), speed_ * 0.25 / step_seconds);
        sim_seconds_measure += static_cast<double>(ran) * step_seconds;
        const double since = std::chrono::duration<double>(now - last_measure).count();
        if (since >= 1.0) {
            std::lock_guard<std::mutex> c(cache_mutex_);
            achieved_ = sim_seconds_measure / since;
            sim_seconds_measure = 0;
            last_measure = now;
        }
        if (now - last_publish >= std::chrono::milliseconds(50) || (ran > 0 && !running_)) {
            std::lock_guard<std::mutex> lock(mutex_);
            publish();
            last_publish = now;
        }
        if (ran == 0) std::this_thread::sleep_for(std::chrono::milliseconds(running_ ? 2 : 15));
    }
}

} // namespace npc::server
