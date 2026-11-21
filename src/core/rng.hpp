#pragma once
// Deterministic, portable random numbers. std:: distributions are implementation-defined, so the
// engine uses its own to guarantee identical runs on Linux, Windows and CI for the same seed.

#include <cmath>
#include <cstdint>
#include <string_view>

namespace npc {

constexpr std::uint64_t splitmix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

/// FNV-1a, used to derive named random streams ("population", "events", ...) from one seed.
constexpr std::uint64_t hash_name(std::string_view s) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (char c : s) { h ^= static_cast<unsigned char>(c); h *= 0x100000001b3ull; }
    return h;
}

constexpr std::uint64_t hash_combine(std::uint64_t a, std::uint64_t b) {
    std::uint64_t s = a ^ (b + 0x9E3779B97F4A7C15ull + (a << 6) + (a >> 2));
    return splitmix64(s);
}

/// xoshiro256** generator.
class Rng {
public:
    Rng() : Rng(42) {}
    explicit Rng(std::uint64_t seed) { reseed(seed); }
    Rng(std::uint64_t seed, std::string_view stream) { reseed(hash_combine(seed, hash_name(stream))); }

    void reseed(std::uint64_t seed) {
        std::uint64_t sm = seed;
        for (auto& v : s_) v = splitmix64(sm);
    }

    std::uint64_t next() {
        const std::uint64_t result = rotl(s_[1] * 5, 7) * 9;
        const std::uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0]; s_[3] ^= s_[1]; s_[1] ^= s_[2]; s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    /// Uniform in [0, 1).
    double uniform() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
    float uniformf() { return static_cast<float>(uniform()); }
    float range(float lo, float hi) { return lo + (hi - lo) * uniformf(); }
    /// Uniform integer in [0, n). n must be > 0.
    std::uint32_t below(std::uint32_t n) { return static_cast<std::uint32_t>((next() >> 32) * n >> 32); }
    int between(int lo, int hi) { return lo + static_cast<int>(below(static_cast<std::uint32_t>(hi - lo + 1))); } // inclusive
    bool chance(float p) { return uniformf() < p; }

    /// Standard normal via Box-Muller (portable, no cached value so the stream stays simple).
    float normal(float mean = 0.f, float sd = 1.f) {
        double u1 = uniform();
        if (u1 < 1e-12) u1 = 1e-12;
        const double u2 = uniform();
        return mean + sd * static_cast<float>(std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2));
    }
    float normal_clamped(float mean, float sd, float lo, float hi) {
        const float v = normal(mean, sd);
        return v < lo ? lo : (v > hi ? hi : v);
    }

    /// Exponential inter-arrival time for a Poisson process with the given rate (events per unit).
    double exponential(double rate) {
        double u = uniform();
        if (u < 1e-12) u = 1e-12;
        return -std::log(u) / rate;
    }

    /// Index chosen proportionally to weights[0..n).
    template <class Weights>
    std::size_t weighted(const Weights& weights) {
        double total = 0;
        for (auto w : weights) total += w > 0 ? w : 0;
        if (total <= 0) return 0;
        double r = uniform() * total;
        std::size_t i = 0;
        for (auto w : weights) {
            if (w > 0 && (r -= w) < 0) return i;
            ++i;
        }
        return i ? i - 1 : 0;
    }

private:
    static constexpr std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    std::uint64_t s_[4]{};
};

} // namespace npc
