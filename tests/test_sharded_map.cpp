#include <concurrent_cache/sharded_map.hpp>
#include <concurrent_cache/lockfree_slot.hpp>

#include <gtest/gtest.h>
#include <atomic>
#include <random>
#include <thread>
#include <vector>

using namespace concurrent_cache;

// ---- concurrent correctness -----------------------------------------------

TEST(sharded_map_concurrent_test, parallel_insert_unique_keys) {
    sharded_map<int, int> map(8);
    constexpr int kPerThread = 10'000;
    constexpr int kThreads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kPerThread; ++i) {
                int key = t * kPerThread + i;
                map.insert(key, key, std::chrono::seconds(300));
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(map.size(), static_cast<size_t>(kPerThread * kThreads));

    // Verify all keys are reachable
    for (int t = 0; t < kThreads; ++t) {
        for (int i = 0; i < kPerThread; ++i) {
            int key = t * kPerThread + i;
            EXPECT_TRUE(map.contains(key));
        }
    }
}

TEST(sharded_map_concurrent_test, parallel_insert_duplicate_keys) {
    sharded_map<int, int> map(4);
    constexpr int kThreads = 8;
    constexpr int kKey = 42;

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            if (map.insert(kKey, 1, std::chrono::seconds(60))) {
                success_count.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    // Exactly one thread should have succeeded
    EXPECT_EQ(success_count.load(), 1);
    EXPECT_EQ(map.size(), 1);
}

TEST(sharded_map_concurrent_test, mixed_insert_and_contains) {
    sharded_map<int, int> map(8);
    constexpr int kOps = 20'000;
    std::atomic<bool> done{false};
    std::atomic<size_t> errors{0};

    std::vector<std::thread> threads;

    // Inserters
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&, t] {
            std::mt19937 rng(static_cast<unsigned int>(t));
            std::uniform_int_distribution<int> dist(0, kOps - 1);
            for (int i = 0; i < kOps / 2; ++i) {
                int key = dist(rng);
                map.insert(key, key, std::chrono::seconds(60));
            }
        });
    }

    // Checkers
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&] {
            std::mt19937 rng(static_cast<unsigned int>(100 + t));
            std::uniform_int_distribution<int> dist(0, kOps - 1);
            for (int i = 0; i < kOps / 2; ++i) {
                int key = dist(rng);
                // contains should never crash or return garbage
                map.contains(key);
            }
        });
    }

    for (auto& th : threads) th.join();
    SUCCEED();
}

// ---- lockfree_slot concurrent ---------------------------------------------

TEST(lockfree_slot_concurrent_test, parallel_insert_same_slot) {
    // All keys hash to the same slot (hash % 1 == 0 for all)
    lockfree_slot_map<int, std::hash<int>> map(1);
    constexpr int kThreads = 8;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] { map.insert(t); });
    }
    for (auto& th : threads) th.join();

    // At least one thread's key should be visible
    bool found = false;
    for (int t = 0; t < kThreads; ++t) {
        found |= map.contains(t);
    }
    EXPECT_TRUE(found);
}

TEST(sharded_map_concurrent_test, eviction_under_load) {
    sharded_map<int, int> map(8);
    constexpr int kInserts = 50'000;

    // Insert all with 0-second TTL (immediately expire)
    for (int i = 0; i < kInserts; ++i) {
        map.insert(i, i, std::chrono::seconds(0));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // All should be gone
    size_t surviving = 0;
    for (int i = 0; i < kInserts; ++i) {
        if (map.contains(i)) ++surviving;
    }
    EXPECT_EQ(surviving, 0);
}
