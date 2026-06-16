#include <concurrent_cache/sharded_map.hpp>

#include <benchmark/benchmark.h>
#include <atomic>
#include <random>
#include <thread>
#include <vector>

using namespace concurrent_cache;

// Thread-count sweep: measure aggregate throughput as threads increase.
// Each thread performs N insert + contains operations against a sharded_map.

static void BM_ContentionScaling(benchmark::State& state) {
    size_t num_threads = static_cast<size_t>(state.range(0));
    size_t shard_count = static_cast<size_t>(state.range(1));
    size_t ops_per_thread = 10'000;

    // Pre-generate keys
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 1 << 18);
    std::vector<int> keys(ops_per_thread * num_threads);
    for (auto& k : keys) k = dist(rng);

    for (auto _ : state) {
        sharded_map<int, int> map(shard_count);

        auto go = [&](size_t tid) {
            size_t base = tid * ops_per_thread;
            for (size_t i = 0; i < ops_per_thread; ++i) {
                map.insert(keys[base + i], static_cast<int>(keys[base + i]),
                           std::chrono::seconds(300));
            }
            for (size_t i = 0; i < ops_per_thread; ++i) {
                map.contains(keys[base + i]);
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back(go, t);
        }
        for (auto& th : threads) th.join();

        benchmark::DoNotOptimize(map.size());
    }

    state.SetItemsProcessed(state.iterations() * ops_per_thread * num_threads * 2);
    state.counters["threads"] = static_cast<double>(num_threads);
    state.counters["shards"]  = static_cast<double>(shard_count);
}

// Sweep: 1/2/4/8/16/32 threads with proportional shard count (threads × 2)
BENCHMARK(BM_ContentionScaling)
    ->Args({1, 2})
    ->Args({2, 4})
    ->Args({4, 8})
    ->Args({8, 16})
    ->Args({16, 32})
    ->Args({32, 64});

// Fixed shards, varying threads — to observe lock contention
BENCHMARK(BM_ContentionScaling)
    ->Args({1, 8})
    ->Args({4, 8})
    ->Args({8, 8})
    ->Args({16, 8})
    ->Args({32, 8});

BENCHMARK_MAIN();
