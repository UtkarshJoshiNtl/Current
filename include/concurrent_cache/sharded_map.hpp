#pragma once

#include "concurrent_cache/timed_eviction.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace concurrent_cache {

// ---------------------------------------------------------------------------
// sharded_map
//
// A concurrent hash map divided into N independent shards, each protected by
// its own mutex.  Composable with any EvictionPolicy (sequential_eviction or
// heap_eviction) for TTL-based expiry.
//
// Key characteristics:
//   - Shard count is fixed at construction time (default = hardware concurrency)
//   - Lookup:  O(1) hash + O(1) shard-locked probe   (amortized)
//   - Insert:  O(1) hash + O(1) shard-locked insert   (amortized, excluding eviction)
//   - Eviction: policy-dependent (sequential: O(K) amortized; heap: O(K log N))
// ---------------------------------------------------------------------------
template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          template <typename, typename> class EvictionPolicy = sequential_eviction>
class sharded_map {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    // ---- construction ------------------------------------------------------

    explicit sharded_map(size_t shard_count = 0)
        : _shards(shard_count > 0 ? shard_count : default_shard_count())
        , _size(0)
    {
        for (auto& s : _shards) {
            s.map.reserve(1024);
        }
    }

    // ---- capacity ----------------------------------------------------------

    size_t size() const noexcept { return _size.load(std::memory_order_relaxed); }
    size_t shard_count() const noexcept { return _shards.size(); }

    // ---- insert ------------------------------------------------------------
    //
    // Insert (key, value) with the given TTL.
    // Returns true if the key was newly inserted;
    // returns false if the key already exists (no update).
    // ------------------------------------------------------------------------
    bool insert(const Key& key, Value value, std::chrono::seconds ttl) {
        auto& shard = _shard_for(key);
        std::lock_guard<std::mutex> lock(shard.mtx);

        // Check duplicate
        if (shard.map.count(key)) {
            return false;
        }

        time_point expiry = clock::now() + ttl;

        shard.map[key] = {std::move(value), expiry};
        shard.eviction.insert(key, expiry);
        _size.fetch_add(1, std::memory_order_relaxed);

        // Evict a batch of expired entries while we hold the lock
        shard.eviction.evict_expired(clock::now(), [&](const Key& k) {
            auto it = shard.map.find(k);
            if (it != shard.map.end() && it->second.expiry <= clock::now()) {
                shard.map.erase(it);
                _size.fetch_sub(1, std::memory_order_relaxed);
            }
        });

        return true;
    }

    // ---- contains ----------------------------------------------------------
    //
    // Check whether key exists AND has not expired.
    // Removes the entry if it has expired (lazy cleanup).
    // ------------------------------------------------------------------------
    bool contains(const Key& key) {
        auto& shard = _shard_for(key);
        std::lock_guard<std::mutex> lock(shard.mtx);

        auto it = shard.map.find(key);
        if (it == shard.map.end()) {
            return false;
        }

        if (clock::now() > it->second.expiry) {
            shard.map.erase(it);
            _size.fetch_sub(1, std::memory_order_relaxed);
            return false;
        }

        return true;
    }

    // ---- evict_expired -----------------------------------------------------
    //
    // Explicitly evict all expired entries across all shards.
    // Returns total number of entries evicted.
    // ------------------------------------------------------------------------
    size_t evict_expired() {
        size_t total = 0;
        time_point now = clock::now();

        for (auto& shard : _shards) {
            std::lock_guard<std::mutex> lock(shard.mtx);
            total += shard.eviction.evict_expired(now, [&](const Key& k) {
                auto it = shard.map.find(k);
                if (it != shard.map.end() && it->second.expiry <= now) {
                    shard.map.erase(it);
                    _size.fetch_sub(1, std::memory_order_relaxed);
                }
            });
        }
        return total;
    }

    // ---- statistics (for diagnostics) --------------------------------------

    struct stats {
        size_t shard_count;
        size_t entries;
        size_t min_shard;
        size_t max_shard;
    };

    stats get_stats() const {
        stats s;
        s.shard_count = _shards.size();
        s.entries = _size.load(std::memory_order_relaxed);
        s.min_shard = SIZE_MAX;
        s.max_shard = 0;
        for (const auto& shard : _shards) {
            std::lock_guard<std::mutex> lock(shard.mtx);
            size_t sz = shard.map.size();
            s.min_shard = std::min(s.min_shard, sz);
            s.max_shard = std::max(s.max_shard, sz);
        }
        return s;
    }

private:
    // ---- per-shard data ----------------------------------------------------
    struct entry {
        Value value;
        time_point expiry;
    };

    struct shard {
        mutable std::mutex mtx;
        std::unordered_map<Key, entry, Hash> map;
        EvictionPolicy<Key, clock> eviction;
    };

    std::vector<shard> _shards;
    std::atomic<size_t> _size;

    // ---- helpers -----------------------------------------------------------

    shard& _shard_for(const Key& key) {
        return _shards[Hash{}(key) % _shards.size()];
    }

    static size_t default_shard_count() {
        unsigned hc = std::thread::hardware_concurrency();
        return hc > 0 ? static_cast<size_t>(hc) : 4;
    }
};

} // namespace concurrent_cache
