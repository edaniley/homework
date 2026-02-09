#include <boost/test/unit_test.hpp>
#include <hw/utility/HashArray.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>
#include <random>
#include <barrier>

using namespace hw::utility;

// Define a compliant KeyType
struct TestKey {
    uint64_t id;
    char padding[8]; // Make it slightly larger than just an int

    TestKey() : id(0) { std::memset(padding, 0, sizeof(padding)); }
    explicit TestKey(uint64_t v) : id(v) { std::memset(padding, 0, sizeof(padding)); }

    uint64_t hash() const noexcept {
        // Simple hash for testing
        // Mix bits to ensure good distribution across 7-bit tags
        uint64_t x = id;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
        x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
        x = x ^ (x >> 31);
        return x;
    }

    bool operator==(const TestKey& other) const {
        return id == other.id;
    }
};

BOOST_AUTO_TEST_SUITE(HashArrayTests)

// 1. Basic Single Threaded Functionality
BOOST_AUTO_TEST_CASE(test_hasharray_basic) {
    HashArray<TestKey, int, 16> table;
    int val1 = 100;
    int val2 = 200;

    // Insert
    auto res1 = table.insert(TestKey(1), &val1);
    bool r1 = res1 == HashArray<TestKey, int, 16>::InsertResult::Success;
    BOOST_CHECK(r1);
    
    auto res2 = table.insert(TestKey(2), &val2);
    bool r2 = res2 == HashArray<TestKey, int, 16>::InsertResult::Success;
    BOOST_CHECK(r2);

    // Find
    BOOST_CHECK_EQUAL(table.find(TestKey(1)), &val1);
    BOOST_CHECK_EQUAL(table.find(TestKey(2)), &val2);
    BOOST_CHECK(table.find(TestKey(3)) == nullptr);

    // Duplicate
    auto res3 = table.insert(TestKey(1), &val2);
    bool r3 = res3 == HashArray<TestKey, int, 16>::InsertResult::DuplicateKey;
    BOOST_CHECK(r3);
}

// 2. Concurrent Insert (Disjoint Keys)
BOOST_AUTO_TEST_CASE(test_concurrent_insert_disjoint) {
    constexpr size_t CAPACITY = 4096;
    constexpr size_t THREADS = 4;
    constexpr size_t ITEMS_PER_THREAD = 1000;
    
    HashArray<TestKey, int, CAPACITY> table;
    std::vector<std::thread> threads;
    std::atomic<bool> error{false};
    int val = 42; // Dummy value

    auto worker = [&](int thread_id) {
        for (size_t i = 0; i < ITEMS_PER_THREAD; ++i) {
            uint64_t id = i * THREADS + thread_id; // Unique ID
            if (table.insert(TestKey(id), &val) != HashArray<TestKey, int, CAPACITY>::InsertResult::Success) {
                error = true;
            }
        }
    };

    for (size_t i = 0; i < THREADS; ++i) threads.emplace_back(worker, i);
    for (auto& t : threads) t.join();

    BOOST_CHECK(!error);

    // Verify presence
    for (size_t i = 0; i < THREADS * ITEMS_PER_THREAD; ++i) {
        if (table.find(TestKey(i)) == nullptr) {
            BOOST_ERROR("Missing key: " << i);
        }
    }
}

// 3. Concurrent Read/Write (Reader should see inserted values eventually)
BOOST_AUTO_TEST_CASE(test_concurrent_read_write) {
    constexpr size_t CAPACITY = 4096;
    HashArray<TestKey, int, CAPACITY> table;
    std::atomic<bool> running{true};
    int val = 1;

    std::thread writer([&]() {
        for (size_t i = 0; i < 2000; ++i) {
            table.insert(TestKey(i), &val);
            std::this_thread::sleep_for(std::chrono::microseconds(1)); // Throttle slightly
        }
        running = false;
    });

    std::thread reader([&]() {
        size_t found_count = 0;
        while (running || found_count < 2000) {
            size_t current_found = 0;
            for (size_t i = 0; i < 2000; ++i) {
                if (table.find(TestKey(i)) != nullptr) {
                    current_found++;
                }
            }
            if (!running && current_found == 2000) break;
            
            // Should be monotonic mostly, but let's just ensure no crashes
            std::this_thread::yield();
        }
    });

    writer.join();
    reader.join();
    
    // Final check
    for (size_t i = 0; i < 2000; ++i) {
        BOOST_CHECK(table.find(TestKey(i)) != nullptr);
    }
}

// 4. Stress Test: High Contention (Same Slots)
BOOST_AUTO_TEST_CASE(test_high_contention) {
    // Small capacity to force collisions and probing
    constexpr size_t CAPACITY = 64; 
    constexpr size_t THREADS = 4;
    // Try to insert more items than capacity concurrently to verify TableFull/Busy logic
    
    HashArray<TestKey, size_t, CAPACITY> table;
    std::vector<std::thread> threads;
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> full_count{0};
    std::vector<size_t> values(THREADS * 20); // Storage for values

    auto worker = [&](int id) {
        for (size_t i = 0; i < 20; ++i) {
            // Keys that hash to similar slots
            // With size 64, many keys will collide naturally.
            // We insert random keys.
            uint64_t key_id = (size_t)rand() % 10000; 
            auto res = table.insert(TestKey(key_id), &values[id * 20 + i]);
            
            if (res == HashArray<TestKey, size_t, CAPACITY>::InsertResult::Success) {
                success_count++;
            } else if (res == HashArray<TestKey, size_t, CAPACITY>::InsertResult::TableFull) {
                full_count++;
            }
        }
    };

    for (size_t i = 0; i < THREADS; ++i) threads.emplace_back(worker, i);
    for (auto& t : threads) t.join();

    // Verification
    // Total successful inserts <= CAPACITY
    BOOST_CHECK(success_count <= CAPACITY);
    // Note: Due to duplicates, success_count might be less than table size if we generated dupes.
    
    size_t actual_count = 0;
    table.for_each([&](const TestKey&, size_t*) {
        actual_count++;
    });
    
    BOOST_CHECK_EQUAL(actual_count, success_count);
}

// 5. ForEach Traversal
BOOST_AUTO_TEST_CASE(test_foreach) {
    constexpr size_t CAPACITY = 128;
    HashArray<TestKey, int, CAPACITY> table;
    int val = 1;
    
    for(size_t i=0; i<50; ++i) {
        table.insert(TestKey(i), &val);
    }
    
    std::atomic<size_t> count{0};
    table.for_each([&](const TestKey& k, int* v) {
        count++;
        BOOST_CHECK_EQUAL(v, &val);
    });
    
    BOOST_CHECK_EQUAL(count, 50);
}

BOOST_AUTO_TEST_SUITE_END()
