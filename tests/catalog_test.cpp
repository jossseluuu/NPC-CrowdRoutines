#include "core/json.hpp"
#include "data/catalog.hpp"
#include "test_framework.hpp"

#include <set>

using namespace npc;

namespace {
const data::Catalog& catalog() {
    static data::Catalog c;
    static bool loaded = false;
    if (!loaded) {
        std::string err;
        loaded = c.load(NPC_DATA_DIR, err);
        if (!loaded) std::printf("  catalog load error: %s\n", err.c_str());
    }
    return c;
}
} // namespace

TEST_CASE("data/catalog loads all files") {
    const auto& c = catalog();
    REQUIRE(!c.activities.empty());
    CHECK(c.activities.size() >= 14);
    CHECK(c.substates.size() >= 85);
    CHECK(c.building_types.size() >= 35);
    CHECK(c.districts.size() >= 10);
    CHECK(c.profiles.size() >= 16);
    CHECK(c.archetypes.size() >= 48);
    CHECK(c.events.size() >= 18);
}

TEST_CASE("data/every activity has substates that point back to it") {
    const auto& c = catalog();
    REQUIRE(!c.activities.empty());
    std::size_t total = 0;
    for (const auto& a : c.activities) {
        CHECK(!a.substates.empty());
        for (int s : a.substates) {
            CHECK(c.substates[static_cast<std::size_t>(s)].activity == a.index);
            ++total;
        }
        if (a.choosable && a.location == data::LocationKind::Venues) CHECK(!a.venues.empty() || a.friend_home || a.family_home || a.own_home);
    }
    CHECK(total == c.substates.size());
}

TEST_CASE("data/opening hours handle late nights and closed days") {
    const auto& c = catalog();
    REQUIRE(!c.building_types.empty());
    const auto& bar = c.building_types[static_cast<std::size_t>(c.building_index("bar"))];
    CHECK(bar.is_open(2, 22.0));
    CHECK(bar.is_open(3, 1.0));   // Thursday 01:00, still Wednesday's opening
    CHECK(!bar.is_open(3, 9.0));
    const auto& club = c.building_types[static_cast<std::size_t>(c.building_index("nightclub"))];
    CHECK(!club.is_open(1, 23.9)); // Tuesday closed
    CHECK(club.is_open(4, 23.9));  // Friday night
    CHECK(club.is_open(5, 3.0));   // Saturday early morning, Friday's opening
    const auto& office = c.building_types[static_cast<std::size_t>(c.building_index("office_small"))];
    CHECK(office.is_open(0, 9.0));
    CHECK(!office.is_open(6, 12.0));
    CHECK(c.building_types[static_cast<std::size_t>(c.building_index("hospital"))].is_open(6, 3.0));
}

TEST_CASE("data/translations cover every id in both languages") {
    const auto& c = catalog();
    REQUIRE(!c.activities.empty());
    for (const char* lang : {"es", "en"}) {
        const auto r = json::parse_file(std::string(NPC_DATA_DIR) + "/i18n/" + lang + ".json");
        REQUIRE(r.ok());
        const auto& t = r.value;
        int missing = 0;
        auto check = [&](const char* section, const std::string& id) {
            if (t[section].str(id).empty()) {
                if (++missing <= 5) std::printf("  missing %s translation %s.%s\n", lang, section, id.c_str());
            }
        };
        for (const auto& a : c.activities) check("activity", a.id);
        for (const auto& s : c.substates) check("substate", s.id);
        for (const auto& b : c.building_types) check("building", b.id);
        for (const auto& d : c.districts) check("district", d.id);
        for (const auto& p : c.profiles) check("profile", p.id);
        for (const auto& a : c.archetypes) check("archetype", a.id);
        for (const auto& e : c.events) check("event", e.id);
        for (auto n : data::kNeedNames) check("need", n);
        for (auto tr : data::kTraitNames) check("trait", tr);
        CHECK(missing == 0);
    }
}

TEST_CASE("data/catalog json exposes the hierarchy") {
    const auto& c = catalog();
    REQUIRE(!c.activities.empty());
    const auto j = c.to_json();
    CHECK(j["activities"].size() == c.activities.size());
    std::set<long long> seen;
    for (const auto& a : j["activities"].as_array())
        for (const auto& s : a["substates"].as_array()) seen.insert(s.integer("index", -1));
    CHECK(seen.size() == c.substates.size());
}

TEST_CASE("data/invalid references are reported") {
    data::Catalog bad;
    std::string err;
    CHECK(!bad.load("/nonexistent-dir", err));
    CHECK(err.find("cannot open") != std::string::npos);
}
