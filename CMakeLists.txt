cmake_minimum_required(VERSION 3.20)
project(wildboids LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizers in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# --- Dependencies via FetchContent ---
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(Catch2 json)

# --- Simulation library (no graphics deps) ---
add_library(wildboids_sim
    src/simulation/rigid_body.cpp
    src/simulation/boid.cpp
    src/simulation/world.cpp
    src/io/boid_spec.cpp
)

target_include_directories(wildboids_sim PUBLIC src)
target_link_libraries(wildboids_sim PUBLIC nlohmann_json::nlohmann_json)

# --- Tests ---
add_executable(wildboids_tests
    tests/test_vec2.cpp
    tests/test_rigid_body.cpp
    tests/test_thruster.cpp
    tests/test_boid.cpp
    tests/test_boid_spec.cpp
    tests/test_world.cpp
)

target_link_libraries(wildboids_tests PRIVATE wildboids_sim Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(wildboids_tests)

# --- GUI application (SDL3) ---
find_package(SDL3 REQUIRED)

add_executable(wildboids
    src/main.cpp
    src/display/renderer.cpp
    src/display/app.cpp
)

target_link_libraries(wildboids PRIVATE wildboids_sim SDL3::SDL3)
