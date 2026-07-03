#pragma once

/**
 * @file timed_eviction.hpp
 * @brief TTL eviction policies for concurrent_cache.
 *
 * Provides two eviction policy classes that can be composed with
 * sharded_map to manage entry expiry:
 *   - sequential_eviction: deque-based, O(1) amortized eviction
 *   - heap_eviction:       min-heap-based, O(log N) insert, O(K log N) eviction
 */

#include <chrono>
#include <deque>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <vector>

namespace concurrent_cache {

// ---------------------------------------------------------------------------
// Sequential eviction policy  (deque-based, O(1) amortized eviction)
//
// Mirrors the Python OrderedDict approach: insert order is preserved,
// expired entries are popped from the front.  Amortized O(1) per insert
// when entries are inserted in (roughly) chronological order.
// ---------------------------------------------------------------------------

/**
 * @brief Deque-based TTL eviction policy.
 *
 * Preserves insertion order. Expired entries are evicted from the front
 * in O(1) amortized time. Best choice when entries are inserted in
 * roughly chronological order (the common case).
 *
 * @tparam Key   Entry key type.
 * @tparam Clock Clock type (default: std::chrono::steady_clock).
 */
template <typename Key, typename Clock = std::chrono::steady_clock>
class sequential_eviction {
public:
    using time_point = typename Clock::time_point;

    /**
     * @brief Record a new entry for eviction tracking.
     * @param key    The entry key.
     * @param expiry The time point at which the entry expires.
     *
     * Complexity: O(1) amortized.
     */
    void insert(Key key, time_point expiry) {
        _queue.emplace_back(expiry, std::move(key));
    }

    /**
     * @brief Remove all entries whose expiry <= now.
     * @tparam F A callable `void(Key)` invoked once per evicted key.
     * @param now  The current time point.
     * @param f    Functor receiving each evicted key.
     * @return Number of entries evicted.
     *
     * Complexity: O(K) where K is the number of expired entries.
     */
    template <typename F>
    size_t evict_expired(time_point now, F&& f) {
        size_t count = 0;
        while (!_queue.empty() && _queue.front().first <= now) {
            f(std::move(_queue.front().second));
            _queue.pop_front();
            ++count;
        }
        return count;
    }

    /** @brief True if no entries are being tracked. */
    bool empty() const noexcept { return _queue.empty(); }

    /** @brief Number of entries currently tracked. */
    size_t size() const noexcept { return _queue.size(); }

private:
    std::deque<std::pair<time_point, Key>> _queue;
};

// ---------------------------------------------------------------------------
// Heap eviction policy  (min-heap, O(log N) insert, O(K log N) eviction)
//
// Uses a binary min-heap keyed by expiry time.  Inserts are O(log N)
// instead of O(1), but eviction of K entries is O(K log N).  Better
// when entries arrive far out of chronological order.
// ---------------------------------------------------------------------------

/**
 * @brief Min-heap-based TTL eviction policy.
 *
 * Maintains entries in a binary min-heap keyed by expiry time.
 * Insert is O(log N); eviction of K entries is O(K log N).
 * Recommended when entries may arrive far out of chronological order.
 *
 * @tparam Key   Entry key type.
 * @tparam Clock Clock type (default: std::chrono::steady_clock).
 */
template <typename Key, typename Clock = std::chrono::steady_clock>
class heap_eviction {
public:
    using time_point = typename Clock::time_point;

    /**
     * @brief Record a new entry for eviction tracking.
     * @param key    The entry key.
     * @param expiry The time point at which the entry expires.
     *
     * Complexity: O(log N).
     */
    void insert(Key key, time_point expiry) {
        _heap.emplace_back(expiry, std::move(key));
        std::push_heap(_heap.begin(), _heap.end(), std::greater<>{});
    }

    /**
     * @brief Remove all entries whose expiry <= now.
     * @tparam F A callable `void(Key)` invoked once per evicted key.
     * @param now  The current time point.
     * @param f    Functor receiving each evicted key.
     * @return Number of entries evicted.
     *
     * Complexity: O(K log N) where K is the number of expired entries.
     */
    template <typename F>
    size_t evict_expired(time_point now, F&& f) {
        size_t count = 0;
        while (!_heap.empty() && _heap.front().first <= now) {
            f(std::move(_heap.front().second));
            std::pop_heap(_heap.begin(), _heap.end(), std::greater<>{});
            _heap.pop_back();
            ++count;
        }
        return count;
    }

    /** @brief True if no entries are being tracked. */
    bool empty() const noexcept { return _heap.empty(); }

    /** @brief Number of entries currently tracked. */
    size_t size() const noexcept { return _heap.size(); }

private:
    // Min-heap: smallest expiry at front
    std::vector<std::pair<time_point, Key>> _heap;
};

} // namespace concurrent_cache
