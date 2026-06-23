#include <concurrent_cache/sharded_map.hpp>

#include <benchmark/benchmark.h>
#include <cstdio>
#include <random>
#include <vector>

// NOTE: Memory footprint is measured externally via /usr/bin/time -v or
// malloc_stats.  This benchmark reports "entries added / notional bytes"
// estimates based on shard map size.  For precise malloc-level analysis,
// run under jemalloc's --mallctl interface or use heaptrack.

using namespace concurrent_cache;

static void BM_MemoryFootprint(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    size_t shards = static_cast<size_t>(state.range(1));

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 1 << 24);

    for (auto _ : state) {
        sharded_map<int, int> map(shards);

        for (size_t i = 0; i < n; ++i) {
            map.insert(dist(rng), 1, std::chrono::seconds(300));
        }

        benchmark::DoNotOptimize(map);
        state.SetLabel(
            ("shards=" + std::to_string(shards) + " entries=" + std::to_string(n)).c_str());
    }

    // Rough estimate: each entry = key(4) + value(4) + expiry(8) + hash table overhead (~32)
    //   = ~48 bytes per entry in the map
    // Plus the eviction queue: key(4) + timepoint(8) + deque overhead (~24)
    //   = ~36 bytes per entry in the eviction queue
    // Total ~84 bytes per live entry
    state.counters["est_bytes_per_entry"] = 84;
    state.counters["entries_added"] = static_cast<double>(n);
}

BENCHMARK(BM_MemoryFootprint)->Args({10'000, 8})->Args({100'000, 8})->Args({1'000'000, 8});
BENCHMARK(BM_MemoryFootprint)->Args({10'000, 64})->Args({100'000, 64})->Args({1'000'000, 64});

BENCHMARK_MAIN();
