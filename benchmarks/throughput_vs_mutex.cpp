#include <concurrent_cache/sharded_map.hpp>
#include <concurrent_cache/lockfree_slot.hpp>

#include <benchmark/benchmark.h>
#include <random>
#include <thread>
#include <vector>

using namespace concurrent_cache;

// Generate a batch of random keys (same seed for all runs)
static std::vector<int> make_keys(size_t n) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 1 << 20);
    std::vector<int> keys(n);
    for (auto& k : keys) k = dist(rng);
    return keys;
}

// ---- 1-shard (single mutex, no sharding) vs 8-shard vs 64-shard ----------

static void BM_ShardedMap_1shard(benchmark::State& state) {
    auto keys = make_keys(state.range(0));
    size_t idx = 0;
    for (auto _ : state) {
        sharded_map<int, int> map(1);
        for (size_t i = 0; i < keys.size(); ++i) {
            map.insert(keys[i], keys[i], std::chrono::seconds(300));
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_ShardedMap_1shard)->Arg(1000)->Arg(10'000)->Arg(100'000);

static void BM_ShardedMap_8shard(benchmark::State& state) {
    auto keys = make_keys(state.range(0));
    for (auto _ : state) {
        sharded_map<int, int> map(8);
        for (size_t i = 0; i < keys.size(); ++i) {
            map.insert(keys[i], keys[i], std::chrono::seconds(300));
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_ShardedMap_8shard)->Arg(1000)->Arg(10'000)->Arg(100'000);

static void BM_ShardedMap_64shard(benchmark::State& state) {
    auto keys = make_keys(state.range(0));
    for (auto _ : state) {
        sharded_map<int, int> map(64);
        for (size_t i = 0; i < keys.size(); ++i) {
            map.insert(keys[i], keys[i], std::chrono::seconds(300));
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_ShardedMap_64shard)->Arg(1000)->Arg(10'000)->Arg(100'000);

// ---- sequential vs heap eviction policy -----------------------------------

static void BM_SequentialEviction(benchmark::State& state) {
    auto keys = make_keys(state.range(0));
    for (auto _ : state) {
        sharded_map<int, int, std::hash<int>, sequential_eviction> map(8);
        for (size_t i = 0; i < keys.size(); ++i) {
            map.insert(keys[i], keys[i], std::chrono::seconds(300));
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_SequentialEviction)->Arg(1000)->Arg(10'000)->Arg(100'000);

static void BM_HeapEviction(benchmark::State& state) {
    auto keys = make_keys(state.range(0));
    for (auto _ : state) {
        sharded_map<int, int, std::hash<int>, heap_eviction> map(8);
        for (size_t i = 0; i < keys.size(); ++i) {
            map.insert(keys[i], keys[i], std::chrono::seconds(300));
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_HeapEviction)->Arg(1000)->Arg(10'000)->Arg(100'000);

// ---- lockfree slot map (lossy, no TTL) ------------------------------------

static void BM_LockfreeSlotMap(benchmark::State& state) {
    auto keys = make_keys(state.range(0));
    for (auto _ : state) {
        lockfree_slot_map<int> map(1 << 20);  // 1M slots
        for (size_t i = 0; i < keys.size(); ++i) {
            map.insert(keys[i]);
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_LockfreeSlotMap)->Arg(1000)->Arg(10'000)->Arg(100'000);

BENCHMARK_MAIN();
