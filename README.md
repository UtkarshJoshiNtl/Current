# concurrent-cache

**Concurrent TTL-backed hash map — a C++17 performance study.**

Three implementations of a time-to-live concurrent associative container,
designed as a standalone C++ exercise in lock contention, eviction policies,
and concurrent data structure design.

## Components

| Header | Description |
|--------|-------------|
| `sharded_map.hpp` | N-shard concurrent hash map, each shard with an independent mutex. Composable with any eviction policy. |
| `timed_eviction.hpp` | Two TTL eviction policies: **sequential** (deque, O(1) amortized, like Python's OrderedDict) and **heap** (min-heap, O(log N)). |
| `lockfree_slot.hpp` | Fixed-capacity lossy lock-free slot map using atomic CAS. No TTL, no locks. |

## Build

```bash
cmake -B build -S concurrent-cache -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build

# Run benchmarks
./build/benchmarks/bench_throughput_vs_mutex
./build/benchmarks/bench_eviction_stress
./build/benchmarks/bench_contention_scaling
./build/benchmarks/bench_memory_footprint
```

## Options

| CMake flag | Default | Description |
|------------|---------|-------------|
| `CC_BUILD_TESTS` | ON | Build Google Test suite |
| `CC_BUILD_BENCHMARKS` | ON | Build Google Benchmark suite |
| `CC_ENABLE_SANITIZERS` | OFF | Enable AddressSanitizer + UndefinedBehaviorSanitizer |
| `CC_ENABLE_TSAN` | OFF | Enable ThreadSanitizer |

## Motivation

This library started as the replay-cache core of an HMAC token authentication
library (see [alpha branch](https://github.com/UtkarshJoshiNtl/EnCrip)).
The Python prototype used a single `threading.Lock` + `OrderedDict` — correct,
but limited to ~5K ops/sec on one core.  This C++ version generalises the
pattern into a reusable concurrent container and benchmarks the performance
impact of each design decision.
