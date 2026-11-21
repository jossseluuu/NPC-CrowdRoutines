cmake_minimum_required(VERSION 3.20)
project(NPC_CrowdRoutines VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
find_package(Threads REQUIRED)

# Warnings and source encoding shared by every target (sources are UTF-8 without BOM).
add_library(npc_warnings INTERFACE)
if(MSVC)
  target_compile_options(npc_warnings INTERFACE /W4 /permissive- /utf-8)
  target_compile_definitions(npc_warnings INTERFACE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(npc_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Core utilities (JSON, RNG, clock, geometry).
add_library(npc_core STATIC src/core/json.cpp)
target_include_directories(npc_core PUBLIC src)
target_link_libraries(npc_core PRIVATE npc_warnings)

# The simulation engine: the single C++ simulation (see docs/architecture/index.md).
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

# HTTP server and runner for the web client.
add_library(npc_server STATIC src/server/http.cpp src/server/runner.cpp src/server/api.cpp)
target_link_libraries(npc_server PUBLIC npc_engine Threads::Threads PRIVATE npc_warnings)
if(WIN32)
  target_link_libraries(npc_server PUBLIC ws2_32)
endif()

# Command line application: serve | run | replay | benchmark | generate-map | validate-map
add_executable(npc_crowd_routines src/app/main.cpp src/app/docs_export.cpp)
target_link_libraries(npc_crowd_routines PRIVATE npc_server npc_warnings)

# Original storage-layout micro-benchmark (OOP vs SoA), kept for ADR-0001.
add_executable(storage_benchmark bench/legacy_storage/main.cpp bench/legacy_storage/simulation.cpp)
target_include_directories(storage_benchmark PRIVATE bench/legacy_storage/include)

# Repository hygiene tool (see docs/development/version-control.md).
add_executable(text_lint tools/text_lint.cpp)
target_link_libraries(text_lint PRIVATE npc_warnings)

# Tests: ctest --preset wsl-release
enable_testing()
add_executable(engine_tests tests/test_main.cpp tests/core_test.cpp tests/catalog_test.cpp tests/world_test.cpp
                            tests/sim_test.cpp tests/server_test.cpp)
target_link_libraries(engine_tests PRIVATE npc_server npc_warnings)
target_include_directories(engine_tests PRIVATE tests)
target_compile_definitions(engine_tests PRIVATE NPC_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME engine COMMAND engine_tests)
add_test(NAME storage_benchmark COMMAND storage_benchmark --agents 1000 --ticks 120 --backend soa --csv)
add_test(NAME cli_validate_map COMMAND npc_crowd_routines validate-map ${CMAKE_SOURCE_DIR}/data/maps/custom/aldea.json
                                       --data ${CMAKE_SOURCE_DIR}/data)
# The binary path is passed as $0 so paths with spaces or "&" work.
add_test(NAME text_lint COMMAND sh -c "git ls-files | \"$0\" --stdin" $<TARGET_FILE:text_lint> WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
