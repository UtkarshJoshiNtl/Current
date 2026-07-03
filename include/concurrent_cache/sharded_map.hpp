#pragma once

/**
 * @file sharded_map.hpp
 * @brief Concurrent sharded hash map with TTL-based expiry.
 *
 * Provides sharded_map: a concurrent hash map divided into N independently
 * locked shards, each composed with a configurable TTL eviction policy.
 */

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

// is_pow2 helper for C++17 ([[likely]] is C++20)
namespace detail {
    inline bool is_pow2(size_t n) noexcept { return (n & (n - 1)) == 0; }
} // namespace detail

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

/**
 * @brief A concurrent, sharded hash map with TTL-based entry expiry.
 *
 * The key space is divided across N shards, each protected by a
 * std::shared_mutex for fine-grained locking.  A configurable eviction
 * policy (sequential_eviction or heap_eviction) tracks expiration.
 *
 * @tparam Key            Entry key type.
 * @tparam Value          Entry value type.
 * @tparam Hash           Hash functor (default: std::hash<Key>).
 * @tparam EvictionPolicy Eviction policy template (default: sequential_eviction).
 *
 * Thread safety:
 * - Concurrent insert, contains, and evict_expired calls are safe from
 *   any number of threads.
 * - size() and get_stats() return approximate values.
 */
template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          template <typename, typename> class EvictionPolicy = sequential_eviction>
class sharded_map {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    // ---- construction ------------------------------------------------------

    /**
     * @brief Construct a sharded map.
     * @param shard_count Number of shards (default: std::thread::hardware_concurrency()
     *                    or 4 if hardware concurrency is not detectable).
     *
     * Each shard's internal unordered_map is pre-reserved with 1024 buckets.
     */
    explicit sharded_map(size_t shard_count = 0)
        : _shards(shard_count > 0 ? shard_count : default_shard_count())
        , _size(0)
    {
        for (auto& s : _shards) {
            s.map.reserve(1024);
        }
    }

    // ---- capacity ----------------------------------------------------------

    /** @brief Approximate number of entries currently stored (relaxed atomic read). */
    size_t size() const noexcept { return _size.load(std::memory_order_relaxed); }

    /** @brief Number of shards. */
    size_t shard_count() const noexcept { return _shards.size(); }

    // ---- insert ------------------------------------------------------------
    //
    // Insert (key, value) with the given TTL.
    // Returns true if the key was newly inserted;
    // returns false if the key already exists (no update).
    // ------------------------------------------------------------------------

    /**
     * @brief Insert a key-value pair with a TTL.
     * @param key   The key to insert.
     * @param value The value to associate with the key.
     * @param ttl   Time-to-live from now.
     * @return true  if the key was newly inserted.
     * @return false if the key already exists (no update performed).
     *
     * Acquires an exclusive lock on the target shard.  After insertion,
     * eagerly evicts any already-expired entries in that shard.
     *
     * Thread safety: safe for concurrent calls (shard-level locking).
     */
    bool insert(const Key& key, Value value, std::chrono::seconds ttl) {
        auto& shard = _shard_for(key);
        std::lock_guard<std::shared_mutex> lock(shard.mtx);

        if (shard.map.count(key)) {
            return false;
        }

        time_point expiry = clock::now() + ttl;

        shard.map[key] = {std::move(value), expiry};
        shard.eviction.insert(key, expiry);
        _size.fetch_add(1, std::memory_order_relaxed);

        // Eviction policy guarantees only expired entries are passed.
        shard.eviction.evict_expired(clock::now(), [&](const Key& k) {
            auto it = shard.map.find(k);
            if (it != shard.map.end()) {
                shard.map.erase(it);
                _size.fetch_sub(1, std::memory_order_relaxed);
            }
        });

        return true;
    }

    // ---- contains ----------------------------------------------------------
    //
    // Check whether key exists AND has not expired.
    // Uses shared_lock for concurrent readers.
    // Removes the entry if it has expired (lazy cleanup).
    // ------------------------------------------------------------------------

    /**
     * @brief Check if a key exists and has not expired.
     * @param key The key to look up.
     * @return true if the key exists and is not yet expired.
     * @return false if the key does not exist or has expired (lazily removed).
     *
     * Uses a shared_lock for concurrent readers.  If the entry is found but
     * expired, the lock is upgraded to exclusive and the entry is erased
     * (lazy cleanup).
     *
     * Thread safety: safe for concurrent calls.
     */
    bool contains(const Key& key) {
        auto& shard = _shard_for(key);
        {
            std::shared_lock<std::shared_mutex> lock(shard.mtx);
            auto it = shard.map.find(key);
            if (it == shard.map.end()) {
                return false;
            }
            if (clock::now() <= it->second.expiry) {
                return true;
            }
        }
        // Upgrade to exclusive lock for erase (shared lock is now released)
        std::lock_guard<std::shared_mutex> ex_lock(shard.mtx);
        auto it = shard.map.find(key);
        if (it != shard.map.end() && clock::now() > it->second.expiry) {
            shard.map.erase(it);
            _size.fetch_sub(1, std::memory_order_relaxed);
        }
        return false;
    }

    // ---- evict_expired -----------------------------------------------------
    //
    // Explicitly evict all expired entries across all shards.
    // Returns total number of entries evicted.
    // ------------------------------------------------------------------------

    /**
     * @brief Evict all expired entries across every shard.
     * @return Total number of entries evicted.
     *
     * Iterates all shards under exclusive lock, evicts expired entries,
     * and shrinks any shard whose map is less than 25% full.
     *
     * Thread safety: safe for concurrent calls.
     */
    size_t evict_expired() {
        size_t total = 0;
        time_point now = clock::now();

        for (auto& shard : _shards) {
            std::lock_guard<std::shared_mutex> lock(shard.mtx);
            total += shard.eviction.evict_expired(now, [&](const Key& k) {
                auto it = shard.map.find(k);
                if (it != shard.map.end()) {
                    shard.map.erase(it);
                    _size.fetch_sub(1, std::memory_order_relaxed);
                }
            });
            // Shrink if map becomes sparse after mass eviction
            if (shard.map.size() < shard.map.bucket_count() / 4) {
                shard.map.rehash(0);
            }
        }
        return total;
    }

    // ---- statistics (for diagnostics) --------------------------------------

    /** @brief Diagnostic statistics for a sharded_map. */
    struct stats {
        size_t shard_count;  ///< Total number of shards.
        size_t entries;      ///< Approximate total entries.
        size_t min_shard;    ///< Smallest shard size.
        size_t max_shard;    ///< Largest shard size.
    };

    /**
     * @brief Collect approximate diagnostic statistics.
     * @return A stats struct populated with current values.
     *
     * Locks every shard sequentially (blocking).  Intended for diagnostics
     * and monitoring, not hot paths.
     */
    stats get_stats() const {
        stats s;
        s.shard_count = _shards.size();
        s.entries = _size.load(std::memory_order_relaxed);
        s.min_shard = 0;
        s.max_shard = 0;
        bool first = true;
        for (const auto& shard : _shards) {
            std::lock_guard<std::shared_mutex> lock(shard.mtx);
            size_t sz = shard.map.size();
            if (first) {
                s.min_shard = sz;
                first = false;
            } else {
                s.min_shard = std::min(s.min_shard, sz);
            }
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
        mutable std::shared_mutex mtx;
        std::unordered_map<Key, entry, Hash> map;
        EvictionPolicy<Key, clock> eviction;
    };

    std::vector<shard> _shards;
    std::atomic<size_t> _size;

    // ---- helpers -----------------------------------------------------------

    /** @brief Select the shard for a given key via hash. */
    shard& _shard_for(const Key& key) {
        size_t n = _shards.size();
        size_t h = Hash{}(key);
        if (detail::is_pow2(n))
            return _shards[h & (n - 1)];
        return _shards[h % n];
    }

    /** @brief Determine default shard count from hardware concurrency. */
    static size_t default_shard_count() {
        unsigned hc = std::thread::hardware_concurrency();
        return hc > 0 ? static_cast<size_t>(hc) : 4;
    }
};

} // namespace concurrent_cache
