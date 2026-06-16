#include <concurrent_cache/timed_eviction.hpp>

#include <benchmark/benchmark.h>
#include <chrono>
#include <random>
#include <vector>

using namespace concurrent_cache;

// Benchmark the cost of evicting expired entries under both policies.
//
// Strategy:
//   1. Insert N entries with staggered TTLs (most expire before the eviction call).
//   2. Call evict_expired and measure the time.
//   3. Report time and throughput (entries evicted / second).

template <template <typename, typename> class Eviction>
static void eviction_drain(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> ttl_dist(-300, -1);  // all in the past

    for (auto _ : state) {
        Eviction<int, std::chrono::steady_clock> ev;
        auto t0 = std::chrono::steady_clock::now();

        // Insert N entries, all already expired
        for (size_t i = 0; i < n; ++i) {
            auto expiry = t0 + std::chrono::seconds(ttl_dist(rng));
            ev.insert(static_cast<int>(i), expiry);
        }

        // Measure eviction
        auto start = std::chrono::steady_clock::now();
        size_t evicted = ev.evict_expired(std::chrono::steady_clock::now(), [](int) {});
        auto end = std::chrono::steady_clock::now();

        benchmark::DoNotOptimize(evicted);
        state.SetIterationTime(
            std::chrono::duration<double>(end - start).count());
    }

    state.SetItemsProcessed(state.iterations() * n);
    state.SetLabel(state.name());
}

BENCHMARK_TEMPLATE(eviction_drain, sequential_eviction)
    ->Arg(1'000)
    ->Arg(10'000)
    ->Arg(100'000)
    ->Arg(1'000'000)
    ->UseManualTime();

BENCHMARK_TEMPLATE(eviction_drain, heap_eviction)
    ->Arg(1'000)
    ->Arg(10'000)
    ->Arg(100'000)
    ->Arg(1'000'000)
    ->UseManualTime();

BENCHMARK_MAIN();
