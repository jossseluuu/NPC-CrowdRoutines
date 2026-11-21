#pragma once
// Tiny self-registering test framework (no dependencies).
//   TEST_CASE("name") { CHECK(expr); REQUIRE(expr); }
// Run the binary with an optional substring filter: ./engine_tests mapgen

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace npc_test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

struct RequireFailed {};

inline int run_all(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0;
    for (auto& c : registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        ++ran;
        const int before = failures();
        try {
            c.fn();
        } catch (const RequireFailed&) {
        } catch (const std::exception& e) {
            ++failures();
            std::printf("  EXCEPTION in %s: %s\n", c.name, e.what());
        }
        std::printf("%s %s\n", failures() == before ? "[ ok ]" : "[FAIL]", c.name);
    }
    std::printf("\n%d test cases, %d checks, %d failures\n", ran, checks(), failures());
    return failures() == 0 ? 0 : 1;
}

} // namespace npc_test

#define NPC_CAT2(a, b) a##b
#define NPC_CAT(a, b) NPC_CAT2(a, b)
#define TEST_CASE(name)                                                                              \
    static void NPC_CAT(npc_test_fn_, __LINE__)();                                                  \
    static npc_test::Registrar NPC_CAT(npc_test_reg_, __LINE__)(name, NPC_CAT(npc_test_fn_, __LINE__)); \
    static void NPC_CAT(npc_test_fn_, __LINE__)()

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++npc_test::checks();                                                             \
        if (!(cond)) {                                                                    \
            ++npc_test::failures();                                                       \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
        }                                                                                 \
    } while (0)

#define REQUIRE(cond)                                                                     \
    do {                                                                                  \
        ++npc_test::checks();                                                             \
        if (!(cond)) {                                                                    \
            ++npc_test::failures();                                                       \
            std::printf("  REQUIRE FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            throw npc_test::RequireFailed{};                                              \
        }                                                                                 \
    } while (0)
