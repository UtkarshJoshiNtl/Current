#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace concurrent_cache {

// ---------------------------------------------------------------------------
// lockfree_slot_map
//
// A fixed-capacity, lossy, lock-free associative array using atomic CAS.
// Capacity must be a power of two.  Collisions silently evict the prior
// occupant (lossy).  No TTL, no eviction callbacks — designed for a
// hot-path cache where lock contention is the dominant cost.
//
// Thread safety: all operations are lock-free (CAS on atomic<uint64_t>).
// Safe for concurrent insert + lookup from any number of threads.
//
// Key limitation: keys are hashed to slot indices; true key equality is NOT
// checked (this is an associative set, not a full map).  For the replay
// cache use case, false positives are safe (reject a token that hasn't been
// seen) — the correctness cost is a small increase in false replay detections.
// ---------------------------------------------------------------------------
template <typename Key, typename Hash = std::hash<Key>>
class lockfree_slot_map {
public:
    explicit lockfree_slot_map(size_t capacity)
        : _capacity(round_up_pow2(capacity))
        , _mask(_capacity - 1)
        , _slots(_capacity)
    {}

    // ---- insert (CAS-based) ------------------------------------------------
    // Returns true if this thread "won" the slot (either empty or stale).
    // No key-equality check: the slot is simply overwritten.
    bool insert(Key key) noexcept {
        uint64_t h = static_cast<uint64_t>(Hash{}(key));
        size_t idx = h & _mask;
        uint64_t fingerprint = h & HASH_MASK;

        slot_value expected = _slots[idx].data.load(std::memory_order_relaxed);
        // Bump generation (low 8 bits) to prevent immediate ABA reclamation
        slot_value desired = fingerprint | ((expected + 1) & GENERATION_MASK);

        if (_slots[idx].data.compare_exchange_weak(
                expected, desired, std::memory_order_acq_rel))
        {
            return true;
        }
        // CAS failed — slot was claimed by another thread between load and CAS.
        // In a lossy cache this is "insertion failed, slot taken".
        return false;
    }

    // ---- lookup ------------------------------------------------------------
    // Returns true if the slot's hash fingerprint matches (probabilistic).
    // False positives are possible (hash collision) but safe for replay cache.
    bool contains(Key key) const noexcept {
        uint64_t h = static_cast<uint64_t>(Hash{}(key));
        size_t idx = h & _mask;
        uint64_t fingerprint = h & HASH_MASK;
        slot_value val = _slots[idx].data.load(std::memory_order_acquire);
        return (val & HASH_MASK) == fingerprint;
    }

    size_t capacity() const noexcept { return _capacity; }

private:
    using slot_value = uint64_t;

    static constexpr slot_value GENERATION_MASK = 0xFF;  // 8 bits for generation
    static constexpr slot_value HASH_MASK = ~GENERATION_MASK;

    struct alignas(64) slot {
        std::atomic<slot_value> data{0};
    };

    size_t _capacity;
    size_t _mask;
    std::vector<slot> _slots;

    static size_t round_up_pow2(size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }
};

} // namespace concurrent_cache
