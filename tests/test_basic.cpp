#include <concurrent_cache/sharded_map.hpp>
#include <concurrent_cache/lockfree_slot.hpp>

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace concurrent_cache;

// ---- sharded_map basic ----------------------------------------------------

TEST(sharded_map_test, insert_and_contains) {
    sharded_map<int, int> map(4);

    EXPECT_TRUE(map.insert(1, 100, std::chrono::seconds(60)));
    EXPECT_TRUE(map.insert(2, 200, std::chrono::seconds(60)));
    EXPECT_TRUE(map.insert(3, 300, std::chrono::seconds(60)));

    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(2));
    EXPECT_TRUE(map.contains(3));
    EXPECT_FALSE(map.contains(4));
}

TEST(sharded_map_test, insert_duplicate_rejected) {
    sharded_map<int, int> map(2);

    EXPECT_TRUE(map.insert(42, 1, std::chrono::seconds(60)));
    EXPECT_FALSE(map.insert(42, 2, std::chrono::seconds(60)));
}

TEST(sharded_map_test, ttl_expiry) {
    sharded_map<int, int> map(2);

    // Insert with 0-second TTL (already expired)
    EXPECT_TRUE(map.insert(1, 10, std::chrono::seconds(0)));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(map.contains(1));
}

TEST(sharded_map_test, negative_ttl_immediate_expiry) {
    sharded_map<int, int> map(2);

    EXPECT_TRUE(map.insert(1, 10, std::chrono::seconds(-1)));
    EXPECT_FALSE(map.contains(1));
}

TEST(sharded_map_test, size_tracking) {
    sharded_map<int, int> map(4);

    EXPECT_EQ(map.size(), 0);
    map.insert(1, 10, std::chrono::seconds(60));
    EXPECT_EQ(map.size(), 1);
    map.insert(2, 20, std::chrono::seconds(60));
    EXPECT_EQ(map.size(), 2);
}

TEST(sharded_map_test, evict_expired_all) {
    sharded_map<int, int> map(2);

    map.insert(1, 10, std::chrono::seconds(0));
    map.insert(2, 20, std::chrono::seconds(0));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    size_t evicted = map.evict_expired();
    // Entries evicted during insert (lazy); evict_expired finds nothing new.
    EXPECT_EQ(evicted, 0);
    EXPECT_FALSE(map.contains(1));
    EXPECT_FALSE(map.contains(2));
}

// ---- sharded_map with heap_eviction ---------------------------------------

TEST(sharded_map_heap_test, insert_and_contains) {
    sharded_map<int, int, std::hash<int>, heap_eviction> map(4);

    EXPECT_TRUE(map.insert(1, 100, std::chrono::seconds(60)));
    EXPECT_TRUE(map.contains(1));
    EXPECT_FALSE(map.contains(2));
}

TEST(sharded_map_heap_test, ttl_expiry) {
    sharded_map<int, int, std::hash<int>, heap_eviction> map(2);

    EXPECT_TRUE(map.insert(1, 10, std::chrono::seconds(0)));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(map.contains(1));
}

// ---- lockfree_slot basic --------------------------------------------------

TEST(lockfree_slot_test, insert_and_contains) {
    lockfree_slot_map<int> map(1024);

    EXPECT_TRUE(map.insert(42));
    EXPECT_TRUE(map.contains(42));
}

TEST(lockfree_slot_test, capacity_rounds_up) {
    lockfree_slot_map<int> map(1000);
    EXPECT_EQ(map.capacity(), 1024);
}

TEST(lockfree_slot_test, negative_not_contained) {
    lockfree_slot_map<int> map(64);
    EXPECT_FALSE(map.contains(999));
}

// ---- sharded_map stats ----------------------------------------------------

TEST(sharded_map_test, stats_report) {
    sharded_map<int, int> map(8);

    map.insert(1, 1, std::chrono::seconds(60));
    map.insert(2, 2, std::chrono::seconds(60));

    auto s = map.get_stats();
    EXPECT_EQ(s.shard_count, 8);
    EXPECT_EQ(s.entries, 2);
}
