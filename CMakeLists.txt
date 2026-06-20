cmake_minimum_required(VERSION 3.10)
project(HFT_Engine VERSION 3.0.0)

# C++17 for structured bindings, constexpr, std::optional, etc.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Aggressive optimization flags for Release builds
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /Ot /GL /DNDEBUG")
    set(CMAKE_CXX_FLAGS "/W4")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")
endif()

# Header search path
include_directories(include)

# Threads for the multithreaded engine + SPSC concurrency test
find_package(Threads REQUIRED)

# ── Targets ─────────────────────────────────────────────────────

# Main engine demo
add_executable(hft_engine src/main.cpp)

# Unit tests (21 tests)
add_executable(test_engine tests/test_engine.cpp)

# Performance benchmark (2M orders)
add_executable(benchmark tests/benchmark.cpp)

# Research: 3-architecture comparison benchmark
add_executable(bench_compare research/benchmark_comparison.cpp)
target_include_directories(bench_compare PRIVATE research)

# Multithreaded engine: feed thread + matching thread over a lock-free SPSC ring
add_executable(threaded_engine src/threaded_engine.cpp)
target_link_libraries(threaded_engine PRIVATE Threads::Threads)

# SPSC ring concurrency correctness test
add_executable(test_spsc tests/test_spsc.cpp)
target_link_libraries(test_spsc PRIVATE Threads::Threads)