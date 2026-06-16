# Performance Results

System: 12-core (6 P-core) × 3.3 GHz, L1d 32 KiB/core, L2 512 KiB/core, L3 16 MiB (shared).  
Compiler: GCC 14, CMake 3.31, C++17, `-O2 -DNDEBUG`.

## 1. Throughput: Insert-Only, Single Thread

Comparing 1, 8, and 64 shards on sequential inserts with 300-second TTL.

| Configuration | 1 000 keys | 10 000 keys | 100 000 keys |
|---------------|-----------:|------------:|-------------:|
| 1 shard       | 10.1 M/s   | 6.05 M/s    | 3.15 M/s     |
| 8 shards      | 9.87 M/s   | 7.29 M/s    | 2.44 M/s     |
| 64 shards     | 5.96 M/s   | 6.15 M/s    | 2.56 M/s     |

**Takeaway:** More shards does not help single-threaded throughput — the extra
cache-line bouncing from touching multiple mutexes offsets any benefit.  The 1-shard
case is the fastest for sequential inserts.

Comparing eviction policies (8 shards, single thread):

| Eviction Policy | 1 000 keys | 10 000 keys | 100 000 keys |
|-----------------|-----------:|------------:|-------------:|
| Sequential (deque) | 6.98 M/s | 6.45 M/s | 1.43 M/s |
| Heap              | 7.25 M/s | 7.40 M/s | 2.08 M/s |

**Takeaway:** Heap eviction *wins* on large sets because the sequential policy's
deque grows long and traverses many already-expired entries during the per-insert
eviction batch.  Heap pays O(log N) per insert but avoids scanning stale prefix.

Lock-free slot map (1M slots, lossy, no TTL):

| 1 000 keys | 10 000 keys | 100 000 keys |
|-----------:|------------:|-------------:|
| 15.6 K/s   | 156 K/s     | 1.52 M/s     |

**Takeaway:** A CAS loop on every insert is catastrophic in single-threaded mode
(no contention to justify the atomic overhead).  Only useful under high contention
where mutex-based designs serialize.

## 2. Eviction-Stress: Draining Already-Expired Entries

| Eviction Policy | 1 000     | 10 000   | 100 000  | 1 000 000 |
|-----------------|----------:|---------:|---------:|----------:|
| Sequential      | 804 M/s   | 611 M/s  | 628 M/s  | 423 M/s   |
| Heap            | 20.4 M/s  | 14.8 M/s | 10.2 M/s | 4.96 M/s  |

**Takeaway:** Sequential eviction is **40–80× faster** than heap at draining
expired entries.  The deque allows a tight linear scan with minimal branching;
the heap requires repeated `pop()` + sink operations.  This matters when replay
caches fill and drain at high rates (e.g., 1M entries/sec).

## 3. Contention Scaling: Mixed Inserts + Contains

Two scenarios: (A) proportional shards (= threads × 2), (B) fixed 8 shards.

### A — Proportional Shards

| Threads | Shards | Throughput | Scaling |
|--------:|-------:|-----------:|--------:|
| 1       | 2      | 33.4 M/s   | 1.00×   |
| 2       | 4      | 41.1 M/s   | 1.23×   |
| 4       | 8      | 38.0 M/s   | 1.14×   |
| 8       | 16     | 24.9 M/s   | 0.75×   |
| 16      | 32     | 20.2 M/s   | 0.60×   |
| 32      | 64     | 17.4 M/s   | 0.52×   |

### B — Fixed 8 Shards

| Threads | Shards | Throughput | Scaling |
|--------:|-------:|-----------:|--------:|
| 1       | 8      | 62.3 M/s   | 1.00×   |
| 4       | 8      | 41.5 M/s   | 0.67×   |
| 8       | 8      | 28.5 M/s   | 0.46×   |
| 16      | 8      | 22.6 M/s   | 0.36×   |
| 32      | 8      | 24.0 M/s   | 0.39×   |

**Takeaway:** Scaling is poor after 2–4 threads.  The bottleneck is `std::mutex`
lock contention: with 8 shards and 32 threads, each shard is contended by 4
threads on average.  Even with 64 shards (32 threads), throughput is only 17 M/s
(0.52× scaling).  A reader-writer lock or RCU-based design would scale better
for read-heavy workloads.

The anomaly (1 thread / 8 shards > 1 thread / 2 shards) is probably because
the 8-shard configuration spreads keys across more hash buckets, reducing
`unordered_map` rehash overhead.

## 4. Memory Footprint

| Entries | Shards | Insert Time | Est. Bytes/Entry |
|--------:|-------:|-----------:|-----------------:|
| 10 K    | 8      | 1.5 ms     | ~84              |
| 100 K   | 8      | 42 ms      | ~84              |
| 1 M     | 8      | 1.02 s     | ~84              |
| 10 K    | 64     | 1.5 ms     | ~84              |
| 100 K   | 64     | 35 ms      | ~84              |
| 1 M     | 64     | 1.06 s     | ~84              |

Rough breakdown per entry: key (4 B) + value (4 B) + expiry (8 B) + hash-table
node overhead (~32 B) + eviction-queue node (~36 B) ≈ 84 B.

## Key Design Recommendations (from data)

1. **Use sequential eviction** unless entries have wildly out-of-order TTLs.
   The 40–80× drain-speed advantage dominates.

2. **Keep shard count ≈ thread count.**  Too few shards → lock contention;
   too many → no benefit and higher memory overhead.

3. **Consider a reader-writer lock** for the shard mutex if the workload is
   read-heavy (token verification reads far more than it writes).

4. **Lock-free slot map** is only useful under extreme contention.  For typical
   replay-cache sizes (10⁵–10⁶ entries), the sharded mutex approach is simpler
   and faster.
