#include <concurrent_cache/timed_eviction.hpp>

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace concurrent_cache;

// ---- sequential_eviction --------------------------------------------------

TEST(sequential_eviction_test, insert_and_evict_none) {
    sequential_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert(1, t0 + std::chrono::seconds(60));
    ev.insert(2, t0 + std::chrono::seconds(60));

    size_t n = 0;
    size_t evicted = ev.evict_expired(t0, [&](int) { ++n; });

    EXPECT_EQ(evicted, 0);
    EXPECT_EQ(n, 0);
}

TEST(sequential_eviction_test, evict_single_expired) {
    sequential_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert(1, t0);
    ev.insert(2, t0 + std::chrono::seconds(60));

    std::vector<int> evicted_keys;
    size_t count = ev.evict_expired(t0, [&](int k) { evicted_keys.push_back(k); });

    EXPECT_EQ(count, 1);
    ASSERT_EQ(evicted_keys.size(), 1);
    EXPECT_EQ(evicted_keys[0], 1);
}

TEST(sequential_eviction_test, evict_multiple_expired) {
    sequential_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert(1, t0 - std::chrono::seconds(10));
    ev.insert(2, t0 - std::chrono::seconds(5));
    ev.insert(3, t0 + std::chrono::seconds(60));

    std::vector<int> evicted_keys;
    size_t count = ev.evict_expired(t0, [&](int k) { evicted_keys.push_back(k); });

    EXPECT_EQ(count, 2);
    ASSERT_EQ(evicted_keys.size(), 2);
    EXPECT_EQ(evicted_keys[0], 1);
    EXPECT_EQ(evicted_keys[1], 2);
}

TEST(sequential_eviction_test, evict_all_expired) {
    sequential_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert(1, t0 - std::chrono::seconds(1));
    ev.insert(2, t0 - std::chrono::seconds(1));

    EXPECT_EQ(ev.evict_expired(t0, [](int) {}), 2);
    EXPECT_TRUE(ev.empty());
}

TEST(sequential_eviction_test, size_tracking) {
    sequential_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    EXPECT_TRUE(ev.empty());
    EXPECT_EQ(ev.size(), 0);

    ev.insert(1, t0 + std::chrono::seconds(60));
    EXPECT_EQ(ev.size(), 1);

    ev.insert(2, t0 + std::chrono::seconds(60));
    EXPECT_EQ(ev.size(), 2);

    ev.evict_expired(t0, [](int) {});
    EXPECT_EQ(ev.size(), 2);  // none expired yet
}

// ---- heap_eviction --------------------------------------------------------

TEST(heap_eviction_test, insert_and_evict_none) {
    heap_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert(1, t0 + std::chrono::seconds(60));
    EXPECT_EQ(ev.evict_expired(t0, [](int) {}), 0);
}

TEST(heap_eviction_test, evict_expired_out_of_order) {
    heap_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    // Insert out of chronological order — heap handles this correctly
    ev.insert(3, t0 + std::chrono::seconds(60));
    ev.insert(1, t0 - std::chrono::seconds(10));
    ev.insert(2, t0 - std::chrono::seconds(5));

    std::vector<int> evicted_keys;
    size_t count = ev.evict_expired(t0, [&](int k) { evicted_keys.push_back(k); });

    EXPECT_EQ(count, 2);
    ASSERT_EQ(evicted_keys.size(), 2);
    // Should evict in expiry order (1 before 2), not insertion order
    EXPECT_EQ(evicted_keys[0], 1);
    EXPECT_EQ(evicted_keys[1], 2);
}

TEST(heap_eviction_test, evict_all) {
    heap_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert(1, t0 - std::chrono::seconds(1));
    ev.insert(2, t0 - std::chrono::seconds(1));

    EXPECT_EQ(ev.evict_expired(t0, [](int) {}), 2);
    EXPECT_TRUE(ev.empty());
}

TEST(heap_eviction_test, size_tracking) {
    heap_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    EXPECT_TRUE(ev.empty());
    ev.insert(1, t0 + std::chrono::seconds(60));
    EXPECT_EQ(ev.size(), 1);
}

// ---- edge cases (both policies) -------------------------------------------

TEST(eviction_edge_test, empty_eviction_noop) {
    sequential_eviction<int> seq_ev;
    heap_eviction<int> heap_ev;

    auto now = std::chrono::steady_clock::now();
    EXPECT_EQ(seq_ev.evict_expired(now, [](int) {}), 0);
    EXPECT_EQ(heap_ev.evict_expired(now, [](int) {}), 0);
}

TEST(eviction_edge_test, zero_ttl_immediate_expiry) {
    sequential_eviction<int> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert(1, t0);
    EXPECT_EQ(ev.evict_expired(t0, [](int) {}), 1);
}

TEST(eviction_edge_test, string_keys) {
    sequential_eviction<std::string> ev;
    auto t0 = std::chrono::steady_clock::now();

    ev.insert("alpha", t0 - std::chrono::seconds(1));
    ev.insert("beta", t0 + std::chrono::seconds(60));

    std::vector<std::string> evicted;
    EXPECT_EQ(ev.evict_expired(t0, [&](const std::string& k) { evicted.push_back(k); }), 1);
    ASSERT_EQ(evicted.size(), 1);
    EXPECT_EQ(evicted[0], "alpha");
}
