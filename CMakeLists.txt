cmake_minimum_required(VERSION 3.20)
project(NPC_CrowdRoutines VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings and source encoding shared by every target (sources are UTF-8 without BOM).
add_library(npc_warnings INTERFACE)
if(MSVC)
  target_compile_options(npc_warnings INTERFACE /W4 /permissive- /utf-8)
else()
  target_compile_options(npc_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Core utilities (JSON, RNG, clock) shared by every engine module.
add_library(npc_core STATIC src/core/json.cpp)
target_include_directories(npc_core PUBLIC src)
target_link_libraries(npc_core PRIVATE npc_warnings)

# Simulation engine: every module of the single C++ simulation (see docs/architecture).
add_library(npc_engine STATIC
  src/data/catalog.cpp
  src/world/city_map.cpp
  src/world/mapgen.cpp
  src/world/navigation.cpp
  src/population/agents.cpp
  src/population/generator.cpp
  src/behavior/behavior.cpp
  src/behavior/movement.cpp
  src/events/event_system.cpp
  src/sim/state.cpp
  src/sim/telemetry.cpp
  src/sim/simulation.cpp)
target_include_directories(npc_engine PUBLIC src)
target_link_libraries(npc_engine PUBLIC npc_core PRIVATE npc_warnings)

# Repository hygiene tool (see docs/development/version-control.md).
add_executable(text_lint tools/text_lint.cpp)
target_link_libraries(text_lint PRIVATE npc_warnings)

# World + simulation systems (buildings, traffic, personality). Stage-3 modules, not yet driven by the
# benchmark loop, but always compiled and tested so they cannot silently break again.
add_library(npc_systems STATIC
  src/world/building.cpp
  src/simulation/systems/building_system.cpp
  src/simulation/systems/traffic_system.cpp)
target_include_directories(npc_systems PUBLIC include src)
target_link_libraries(npc_systems PRIVATE npc_warnings)

# Native benchmark executable.
add_executable(npc_crowd_routines src/app/main.cpp src/core/simulation.cpp)
target_include_directories(npc_crowd_routines PRIVATE include)
target_link_libraries(npc_crowd_routines PRIVATE npc_warnings)
if(MSVC)
  target_link_libraries(npc_crowd_routines PRIVATE shell32)
endif()

# Keep the browser visualization beside the executable on every build, including
# when only HTML/JavaScript files changed and the native target is up to date.
add_custom_target(copy_viewer ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/viewer
          $<TARGET_FILE_DIR:npc_crowd_routines>/viewer
  COMMENT "Copying browser viewer next to the executable"
  VERBATIM)
add_dependencies(copy_viewer npc_crowd_routines)

# Tests: ctest -C Release
enable_testing()
add_executable(systems_test tests/systems_test.cpp)
target_link_libraries(systems_test PRIVATE npc_systems npc_warnings)
add_test(NAME systems COMMAND systems_test)
add_executable(engine_tests tests/test_main.cpp tests/core_test.cpp tests/catalog_test.cpp tests/world_test.cpp tests/sim_test.cpp)
target_link_libraries(engine_tests PRIVATE npc_engine npc_warnings)
target_include_directories(engine_tests PRIVATE tests)
target_compile_definitions(engine_tests PRIVATE NPC_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME engine COMMAND engine_tests)
add_test(NAME benchmark_soa COMMAND npc_crowd_routines --benchmark --agents 1000 --ticks 120 --backend soa)
add_test(NAME benchmark_oop COMMAND npc_crowd_routines --benchmark --agents 1000 --ticks 120 --backend oop --csv)
