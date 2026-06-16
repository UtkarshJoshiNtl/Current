#pragma once

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
template <typename Key, typename Clock = std::chrono::steady_clock>
class sequential_eviction {
public:
    using time_point = typename Clock::time_point;

    void insert(Key key, time_point expiry) {
        _queue.emplace_back(expiry, std::move(key));
    }

    // Remove all entries whose expiry <= now, calling functor f(key) for each.
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

    bool empty() const noexcept { return _queue.empty(); }
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
template <typename Key, typename Clock = std::chrono::steady_clock>
class heap_eviction {
public:
    using time_point = typename Clock::time_point;

    void insert(Key key, time_point expiry) {
        _heap.emplace_back(expiry, std::move(key));
        std::push_heap(_heap.begin(), _heap.end(), std::greater<>{});
    }

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

    bool empty() const noexcept { return _heap.empty(); }
    size_t size() const noexcept { return _heap.size(); }

private:
    // Min-heap: smallest expiry at front
    std::vector<std::pair<time_point, Key>> _heap;
};

} // namespace concurrent_cache
