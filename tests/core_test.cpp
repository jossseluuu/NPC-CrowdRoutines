#include "core/clock.hpp"
#include "core/json.hpp"
#include "core/rng.hpp"
#include "test_framework.hpp"

#include <cmath>

using namespace npc;

TEST_CASE("core/json parse and dump round trip") {
    const auto r = json::parse(R"({
        // comments are allowed in data files
        "name": "Ciudad éxito", "n": 3, "pi": 3.25, "ok": true, "none": null,
        "list": [1, 2, 3,], "nested": {"a": [{"b": "c"}]}
    })");
    REQUIRE(r.ok());
    const auto& v = r.value;
    CHECK(v.str("name") == "Ciudad éxito");
    CHECK(v.integer("n", 0) == 3);
    CHECK(std::abs(v.num("pi", 0) - 3.25) < 1e-12);
    CHECK(v.boolean("ok", false));
    CHECK(v["none"].is_null());
    CHECK(v["list"].size() == 3);
    CHECK(v["nested"]["a"][0].str("b") == "c");
    CHECK(v["missing"]["deeper"].is_null());

    const auto again = json::parse(v.dump(2));
    REQUIRE(again.ok());
    CHECK(again.value == v);
    CHECK(json::parse(v.dump()).value == v);
}

TEST_CASE("core/json errors report position") {
    const auto r = json::parse("{\n  \"a\": [1, 2,, 3]\n}");
    CHECK(!r.ok());
    CHECK(r.error.find("line 2") != std::string::npos);
    CHECK(!json::parse("[1, 2").ok());
    CHECK(!json::parse("\"unterminated").ok());
    CHECK(!json::parse("{} extra").ok());
}

TEST_CASE("core/json escapes control characters and keeps insertion order") {
    json::Value v = json::Value::object();
    v.set("z", "line\nbreak \"quoted\" \t tab");
    v.set("a", 1);
    const std::string s = v.dump();
    CHECK(s.find("\\n") != std::string::npos);
    CHECK(s.find("\\\"quoted\\\"") != std::string::npos);
    CHECK(s.find("\"z\"") < s.find("\"a\""));
    CHECK(json::parse(s).value.str("z") == "line\nbreak \"quoted\" \t tab");
}

TEST_CASE("core/rng is deterministic and streams are independent") {
    Rng a(123, "population"), b(123, "population"), c(123, "events");
    bool same = true, differs = false;
    for (int i = 0; i < 100; ++i) {
        const auto x = a.next(), y = b.next(), z = c.next();
        same &= x == y;
        differs |= x != z;
    }
    CHECK(same);
    CHECK(differs);

    Rng r(7);
    double sum = 0, sq = 0;
    const int n = 20000;
    for (int i = 0; i < n; ++i) {
        const float v = r.normal(0.f, 1.f);
        sum += v;
        sq += v * v;
    }
    CHECK(std::abs(sum / n) < 0.05);
    CHECK(std::abs(sq / n - 1.0) < 0.06);

    int counts[3] = {};
    const float w[3] = {1.f, 2.f, 7.f};
    for (int i = 0; i < 10000; ++i) ++counts[r.weighted(w)];
    CHECK(counts[2] > counts[1] && counts[1] > counts[0]);
    for (int i = 0; i < 1000; ++i) {
        const int k = r.between(3, 5);
        CHECK(k >= 3 && k <= 5);
        if (k < 3 || k > 5) break;
    }
}

TEST_CASE("core/clock formatting and parsing") {
    SimClock c;
    c.start_seconds = 23.5 * 3600;
    c.start_weekday = 4; // Friday
    CHECK(c.day() == 0);
    c.tick = 3600; // +1h -> 00:30 Saturday
    CHECK(c.day() == 1);
    CHECK(c.weekday() == 5);
    CHECK(c.is_weekend());
    CHECK(std::abs(c.hour() - 0.5) < 1e-9);
    CHECK(c.label() == "d1 00:30");
    CHECK(SimClock::parse("d1 00:30") == c.seconds());
    CHECK(SimClock::parse("08:15") == 8 * 3600 + 15 * 60);
    CHECK(c.tick_at(c.seconds()) == c.tick);
    CHECK(hour_in_window(23.5, 22, 6));
    CHECK(hour_in_window(1.0, 22, 6));
    CHECK(!hour_in_window(12.0, 22, 6));
}
