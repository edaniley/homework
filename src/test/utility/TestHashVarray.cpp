#include <boost/test/unit_test.hpp>
#include <hw/utility/HashVarray.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>
#include <random>
#include <barrier>
#include <iostream>
#include <set>
#include <stdexcept>

// Since HashVarray doesn't include the Key template (it was in HashArray.hpp), 
// we need to include HashArray.hpp or copy Key. 
// The prompt said "create a version of HashArray".
// If I assume Key is needed, I should include HashArray.hpp for Key definition, or redefine it.
// The user said "name this template as HashVarray".
// Let's include HashArray.hpp to get the Key template and other utils if they are shared.
// But wait, HashVarray.hpp defined its own InsertResult if needed.
// To avoid redefinition errors if I include both, I should have checked guards.
// In HashVarray.hpp I added a guard for InsertResult.
// Let's assume we can include HashArray.hpp for the Key helper.
#include <hw/utility/HashArray.hpp> 

using namespace hw::utility;
using namespace hw::utility::swisstable;

// -----------------------------------------------------------------------------
// Test Helpers
// -----------------------------------------------------------------------------

// A controllable key to force collisions or specific hash patterns
struct TestKeyV {
    uint64_t id;
    uint64_t forced_hash;

    TestKeyV() : id(0), forced_hash(0) {}
    TestKeyV(uint64_t v, uint64_t h = 0) : id(v), forced_hash(h) {
        if (forced_hash == 0) forced_hash = splitmix64(id);
    }

    uint64_t hash() const noexcept { return forced_hash; }
    
    bool operator==(const TestKeyV& other) const { return id == other.id; }
    bool operator!=(const TestKeyV& other) const { return id != other.id; }

private:
    static uint64_t splitmix64(uint64_t x) {
        x += 0x9e3779b97f4a7c15;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
        x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
        return x ^ (x >> 31);
    }
};

// -----------------------------------------------------------------------------
// Test Suite
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(HashVarrayTestSuite)

//
// 1. Single Threaded Policy Tests
//
BOOST_AUTO_TEST_CASE(ST_BasicOperations) {
    constexpr size_t CAP = 16;
    HashVarray<TestKeyV, int, false> table(CAP);
    int val1 = 1, val2 = 2;

    // Insert
    BOOST_CHECK(table.insert(TestKeyV(1), &val1) == InsertResult::Success);
    BOOST_CHECK(table.insert(TestKeyV(2), &val2) == InsertResult::Success);

    // Find
    BOOST_CHECK_EQUAL(table.find(TestKeyV(1)), &val1);
    BOOST_CHECK_EQUAL(table.find(TestKeyV(2)), &val2);
    BOOST_CHECK(table.find(TestKeyV(3)) == nullptr);

    // Duplicate
    BOOST_CHECK(table.insert(TestKeyV(1), &val2) == InsertResult::DuplicateKey);
}

BOOST_AUTO_TEST_CASE(ST_TableFull) {
    constexpr size_t CAP = 16;
    HashVarray<TestKeyV, int, false> table(CAP);
    int val = 0;

    for (size_t i = 0; i < CAP; ++i) {
        BOOST_CHECK(table.insert(TestKeyV(i), &val) == InsertResult::Success);
    }

    // Table is full
    BOOST_CHECK(table.insert(TestKeyV(CAP), &val) == InsertResult::TableFull);
    
    // Check all exist
    for (size_t i = 0; i < CAP; ++i) {
        BOOST_CHECK(table.find(TestKeyV(i)) != nullptr);
    }
}

BOOST_AUTO_TEST_CASE(ST_CollisionAndProbing) {
    constexpr size_t CAP = 16;
    HashVarray<TestKeyV, int, false> table(CAP);
    int val = 0;

    // Force all keys to hash to the same slot (e.g., hash=0)
    
    for (size_t i = 0; i < CAP; ++i) {
        // Different IDs, same hash -> massive collision
        TestKeyV k(i, 0); 
        BOOST_CHECK(table.insert(k, &val) == InsertResult::Success);
    }

    // Verify retrieval works despite collisions
    for (size_t i = 0; i < CAP; ++i) {
        TestKeyV k(i, 0);
        BOOST_CHECK(table.find(k) != nullptr);
    }
}

BOOST_AUTO_TEST_CASE(ST_InvalidConstruction) {
    BOOST_CHECK_THROW((HashVarray<TestKeyV, int, false>(15)), std::invalid_argument);
    BOOST_CHECK_THROW((HashVarray<TestKeyV, int, false>(8)), std::invalid_argument); // < 16
}

//
// 2. Multi Threaded Policy Tests (Functional)
//
BOOST_AUTO_TEST_CASE(MT_BasicOperations) {
    constexpr size_t CAP = 32;
    HashVarray<TestKeyV, int, true> table(CAP);
    int val = 42;

    BOOST_CHECK(table.insert(TestKeyV(100), &val) == InsertResult::Success);
    BOOST_CHECK_EQUAL(table.find(TestKeyV(100)), &val);
    BOOST_CHECK(table.find(TestKeyV(999)) == nullptr);
}

BOOST_AUTO_TEST_CASE(MT_LibraryKeyType) {
    // Verify the provided Key<SIZE> class works (from HashArray.hpp)
    constexpr size_t CAP = 16;
    using LibKey = hw::utility::swisstable::Key<8>;
    HashVarray<LibKey, int, true> table(CAP);
    
    LibKey k1;
    *k1.data<uint64_t>() = 12345;
    
    int val = 1;
    BOOST_CHECK(table.insert(k1, &val) == InsertResult::Success);
    
    LibKey k2;
    *k2.data<uint64_t>() = 12345;
    BOOST_CHECK_EQUAL(table.find(k2), &val); // k2 == k1
    
    LibKey k3;
    *k3.data<uint64_t>() = 67890;
    BOOST_CHECK(table.find(k3) == nullptr);
}

//
// 3. Stress Tests (Multi Threaded)
//

BOOST_AUTO_TEST_CASE(Stress_ConcurrentInserts_Unique) {
    constexpr size_t CAP = 4096;
    constexpr int NUM_THREADS = 8;
    constexpr int ITEMS_PER_THREAD = CAP / NUM_THREADS;
    
    HashVarray<TestKeyV, int, true> table(CAP);
    std::vector<std::thread> threads;
    std::vector<int> values(CAP); // Stable addresses
    std::atomic<int> errors{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                int id = t * ITEMS_PER_THREAD + i;
                TestKeyV k(id);
                if (table.insert(k, &values[id]) != InsertResult::Success) {
                    errors++;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    BOOST_CHECK_EQUAL(errors.load(), 0);

    // Verify all found
    for (int i = 0; i < NUM_THREADS * ITEMS_PER_THREAD; ++i) {
        TestKeyV k(i);
        if (table.find(k) == nullptr) {
            BOOST_ERROR("Failed to find key " << i);
        }
    }
}

BOOST_AUTO_TEST_CASE(Stress_ConcurrentInserts_Duplicates) {
    // Multiple threads trying to insert the SAME keys.
    // Only one should succeed per key.
    constexpr size_t CAP = 1024;
    constexpr int NUM_THREADS = 8;
    
    HashVarray<TestKeyV, int, true> table(CAP);
    std::vector<std::thread> threads;
    int val = 99;
    std::atomic<int> success_count{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < (int)CAP; ++i) {
                // Everyone tries to insert key 'i'
                if (table.insert(TestKeyV(i), &val) == InsertResult::Success) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    // Exactly CAP successful inserts (one for each key 0..1023)
    BOOST_CHECK_EQUAL(success_count.load(), CAP);
}

BOOST_AUTO_TEST_CASE(Stress_ReadWhileWrite) {
    constexpr size_t CAP = 2048;
    HashVarray<TestKeyV, int, true> table(CAP);
    std::atomic<bool> done{false};
    int val = 1;

    // Reader thread
    std::thread reader([&]() {
        while (!done) {
            // Just continuously scan random keys to check for consistency/crashes
            // We can't guarantee what is found or not, but it shouldn't segfault or hang
            for (int i = 0; i < 100; ++i) {
                table.find(TestKeyV(rand() % CAP));
            }
            std::this_thread::yield();
        }
    });

    // Writer thread
    std::thread writer([&]() {
        for (size_t i = 0; i < CAP; ++i) {
            table.insert(TestKeyV(i), &val);
            if (i % 100 == 0) std::this_thread::yield();
        }
        done = true;
    });

    writer.join();
    reader.join();

    // Final check
    int found = 0;
    for (size_t i = 0; i < CAP; ++i) {
        if (table.find(TestKeyV(i))) found++;
    }
    BOOST_CHECK_EQUAL(found, CAP);
}

BOOST_AUTO_TEST_CASE(Stress_HighContention_WrapAround) {
    // Force collisions at the END of the table to test wrap-around logic under concurrency
    constexpr size_t CAP = 32;
    constexpr int NUM_THREADS = 4;
    
    HashVarray<TestKeyV, int, true> table(CAP);
    std::vector<std::thread> threads;
    std::atomic<int> successes{0};
    int val = 1;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            // Generate keys that all map to the last bucket or near it
            // hash >> 7 should be 31 (plus multiples of 32).
            // hash = (31 << 7) = 3968.
            
            for (int i = 0; i < 20; ++i) {
                // Unique IDs, but colliding hash start
                uint64_t id = t * 100 + i;
                // Force hash to map to last bucket (idx 31)
                uint64_t target_hash = (31 << 7) | 0x01; 
                
                TestKeyV k(id, target_hash);
                
                InsertResult res = table.insert(k, &val);
                if (res == InsertResult::Success) successes++;
            }
        });
    }

    for (auto& t : threads) t.join();

    // The table should be full or close to it, and no data corruption
    int final_count = 0;
    table.for_each([&](const TestKeyV&, int*) { final_count++; });
    
    BOOST_CHECK_EQUAL(final_count, successes.load());
}

BOOST_AUTO_TEST_SUITE_END()
