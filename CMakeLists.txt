cmake_minimum_required(VERSION 3.20)
project(concurrent-cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Options ----------------------------------------------------------------
option(CC_BUILD_TESTS     "Build unit tests"        ON)
option(CC_BUILD_BENCHMARKS "Build benchmarks"        ON)
option(CC_ENABLE_SANITIZERS "Enable ASan+UBSan"      OFF)
option(CC_ENABLE_TSAN     "Enable ThreadSanitizer"   OFF)

# --- C++ standard + warnings ------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    add_compile_options(-Werror=return-type -Wno-unused-parameter)
endif()

if(CC_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(CC_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# --- Library (header-only, INTERFACE target) --------------------------------
add_library(concurrent_cache INTERFACE)
target_include_directories(concurrent_cache INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# --- Dependencies -----------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

if(CC_BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

if(CC_BUILD_BENCHMARKS)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

# --- Tests ------------------------------------------------------------------
if(CC_BUILD_TESTS)
    function(add_cc_test name)
        add_executable(${name} tests/${name}.cpp)
        target_link_libraries(${name} PRIVATE concurrent_cache gtest_main)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_cc_test(test_basic)
    add_cc_test(test_sharded_map)
    add_cc_test(test_timed_eviction)
endif()

# --- Benchmarks -------------------------------------------------------------
if(CC_BUILD_BENCHMARKS)
    function(add_cc_benchmark name)
        add_executable(bench_${name} benchmarks/${name}.cpp)
        target_link_libraries(bench_${name} PRIVATE concurrent_cache benchmark::benchmark)
    endfunction()

    add_cc_benchmark(throughput_vs_mutex)
    add_cc_benchmark(eviction_stress)
    add_cc_benchmark(contention_scaling)
    add_cc_benchmark(memory_footprint)
endif()

# --- Convenience target -----------------------------------------------------
add_custom_target(bench ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Build OK."
    COMMENT "Run benchmarks with: ctest (tests) or individual bench_* executables from build/benchmarks/"
)
