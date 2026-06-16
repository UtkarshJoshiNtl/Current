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
        size_t idx = Hash{}(key) & _mask;
        uint64_t key_hash = static_cast<uint64_t>(Hash{}(key));

        // Encode generation in low bits, hash in high bits
        slot_value desired = encode(key_hash, _slots[idx].generation.load(std::memory_order_relaxed) + 1);
        slot_value expected = _slots[idx].data.load(std::memory_order_relaxed);

        // CAS loop: try to claim the slot
        while (true) {
            // If the slot holds a different hash (or is empty), CAS to claim it
            if (_slots[idx].data.compare_exchange_weak(
                    expected, desired, std::memory_order_acq_rel))
            {
                _slots[idx].generation.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            // Slot is held by another thread's hash — treat as occupied;
            // in a lossy cache this is "insertion failed, slot taken"
            return false;
        }
    }

    // ---- lookup ------------------------------------------------------------
    // Returns true if the slot's hash matches (probabilistic).
    // False positives are possible (hash collision) but safe for replay cache.
    bool contains(Key key) const noexcept {
        size_t idx = Hash{}(key) & _mask;
        uint64_t key_hash = static_cast<uint64_t>(Hash{}(key));
        slot_value val = _slots[idx].data.load(std::memory_order_acquire);
        return (val & ~GENERATION_MASK) == (key_hash & ~GENERATION_MASK);
    }

    size_t capacity() const noexcept { return _capacity; }

private:
    using slot_value = uint64_t;

    static constexpr slot_value GENERATION_MASK = 0xFF;  // 8 bits for generation
    static constexpr slot_value HASH_MASK = ~GENERATION_MASK;

    struct alignas(64) slot {
        std::atomic<slot_value> data{0};
        std::atomic<uint64_t> generation{0};
    };

    size_t _capacity;
    size_t _mask;
    mutable std::vector<slot> _slots;

    static size_t round_up_pow2(size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    static slot_value encode(uint64_t hash, uint64_t gen) noexcept {
        return ((hash & HASH_MASK) | (gen & GENERATION_MASK));
    }
};

} // namespace concurrent_cache
