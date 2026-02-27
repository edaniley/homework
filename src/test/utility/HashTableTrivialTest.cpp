
#include <boost/test/unit_test.hpp>
#include <array>
#include <cstddef>
#include <vector>
#include <numeric>
#include <string>

#include <hw/utility/HashTableTrivial.hpp>
#include <hw/utility/Allocator.hpp>

// Dummy KeyType for testing
static constexpr size_t TEST_KEY_SIZE = 8;
using TestKey = std::array<std::byte, TEST_KEY_SIZE>;

// Custom hash for TestKey
namespace std {
    template <>
    struct hash<TestKey> {
        size_t operator()(const TestKey& k) const {
            size_t seed = 0;
            for (std::byte b : k) {
                seed ^= static_cast<size_t>(b) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}

// Dummy PayloadType for testing
using TestPayload = int;

// Define the HashTableTrivial instance for testing
using TestHashTable = hw::utility::HashTableTrivial<TestKey, TestPayload>;

BOOST_AUTO_TEST_SUITE(HashTableTrivialTestSuite)

BOOST_AUTO_TEST_CASE(HT_ConstructionAndEmpty) {
    BOOST_CHECK_THROW(TestHashTable(0), std::invalid_argument);

    TestHashTable ht(10);
    BOOST_CHECK_EQUAL(ht.size(), 0);
    BOOST_CHECK(ht.empty());
}

BOOST_AUTO_TEST_CASE(HT_InsertAndFind) {
    TestHashTable ht(10);
    TestKey key1; std::fill(key1.begin(), key1.end(), static_cast<std::byte>(1));
    TestKey key2; std::fill(key2.begin(), key2.end(), static_cast<std::byte>(2));
    TestKey key3; std::fill(key3.begin(), key3.end(), static_cast<std::byte>(3));

    BOOST_CHECK(ht.insert(key1, 100));
    BOOST_CHECK_EQUAL(ht.size(), 1);
    BOOST_CHECK(!ht.empty());

    BOOST_CHECK(ht.insert(key2, 200));
    BOOST_CHECK_EQUAL(ht.size(), 2);

    BOOST_CHECK(ht.insert(key3, 300));
    BOOST_CHECK_EQUAL(ht.size(), 3);

    // Test finding existing keys
    BOOST_CHECK_EQUAL(*ht.find(key1), 100);
    BOOST_CHECK_EQUAL(*ht.find(key2), 200);
    BOOST_CHECK_EQUAL(*ht.find(key3), 300);

    // Test finding non-existent key
    TestKey nonExistentKey; std::fill(nonExistentKey.begin(), nonExistentKey.end(), static_cast<std::byte>(99));
    BOOST_CHECK(ht.find(nonExistentKey) == nullptr);

    // Test inserting duplicate key
    BOOST_CHECK(!ht.insert(key1, 101)); // Should fail, key already exists
    BOOST_CHECK_EQUAL(ht.size(), 3); // Size should not change
    BOOST_CHECK_EQUAL(*ht.find(key1), 100); // Value should not change
}

BOOST_AUTO_TEST_CASE(HT_Erase) {
    TestHashTable ht(10);
    TestKey key1; std::fill(key1.begin(), key1.end(), static_cast<std::byte>(1));
    TestKey key2; std::fill(key2.begin(), key2.end(), static_cast<std::byte>(2));

    ht.insert(key1, 100);
    ht.insert(key2, 200);

    BOOST_CHECK_EQUAL(ht.size(), 2);

    // Erase existing key
    BOOST_CHECK(ht.erase(key1));
    BOOST_CHECK_EQUAL(ht.size(), 1);
    BOOST_CHECK(ht.find(key1) == nullptr);

    // Erase non-existent key
    TestKey nonExistentKey; std::fill(nonExistentKey.begin(), nonExistentKey.end(), static_cast<std::byte>(99));
    BOOST_CHECK(!ht.erase(nonExistentKey));
    BOOST_CHECK_EQUAL(ht.size(), 1); // Size should not change

    // Erase the last remaining key
    BOOST_CHECK(ht.erase(key2));
    BOOST_CHECK_EQUAL(ht.size(), 0);
    BOOST_CHECK(ht.empty());
}

BOOST_AUTO_TEST_CASE(HT_Clear) {
    TestHashTable ht(10);
    TestKey key1; std::fill(key1.begin(), key1.end(), static_cast<std::byte>(1));
    TestKey key2; std::fill(key2.begin(), key2.end(), static_cast<std::byte>(2));

    ht.insert(key1, 100);
    ht.insert(key2, 200);
    BOOST_CHECK_EQUAL(ht.size(), 2);

    ht.clear();
    BOOST_CHECK_EQUAL(ht.size(), 0);
    BOOST_CHECK(ht.empty());
    BOOST_CHECK(ht.find(key1) == nullptr);
    BOOST_CHECK(ht.find(key2) == nullptr);
}

// Test with a larger number of elements to check bucket distribution
BOOST_AUTO_TEST_CASE(HT_LargeNumberOfElements) {
    static constexpr size_t NUM_ELEMENTS = 1000;
    TestHashTable ht(NUM_ELEMENTS); // Should create enough buckets

    for (int i = 0; i < NUM_ELEMENTS; ++i) {
        TestKey key;
        // Create unique keys for each insertion
        size_t val = i; // Use i directly to make keys unique
        std::memcpy(key.data(), &val, std::min(sizeof(val), TEST_KEY_SIZE));
        BOOST_CHECK_MESSAGE(ht.insert(key, i), "Insertion failed for key " << i);
    }
    BOOST_CHECK_EQUAL(ht.size(), NUM_ELEMENTS);

    for (int i = 0; i < NUM_ELEMENTS; ++i) {
        TestKey key;
        size_t val = i; // Recreate the same unique key
        std::memcpy(key.data(), &val, std::min(sizeof(val), TEST_KEY_SIZE));
        BOOST_CHECK(ht.find(key) != nullptr);
        BOOST_CHECK_EQUAL(*ht.find(key), i);
    }

    // Erase half the elements
    for (int i = 0; i < NUM_ELEMENTS / 2; ++i) {
        TestKey key;
        size_t val = i; // Recreate the same unique key
        std::memcpy(key.data(), &val, std::min(sizeof(val), TEST_KEY_SIZE));
        BOOST_CHECK_MESSAGE(ht.erase(key), "Erasure failed for key " << i);
    }
    BOOST_CHECK_EQUAL(ht.size(), NUM_ELEMENTS / 2);

    // Verify remaining elements
    for (int i = NUM_ELEMENTS / 2; i < NUM_ELEMENTS; ++i) {
        TestKey key;
        size_t val = i; // Recreate the same unique key
        std::memcpy(key.data(), &val, std::min(sizeof(val), TEST_KEY_SIZE));
        BOOST_CHECK(ht.find(key) != nullptr);
        BOOST_CHECK_EQUAL(*ht.find(key), i);
    }

    // Verify erased elements are not found
    for (int i = 0; i < NUM_ELEMENTS / 2; ++i) {
        TestKey key;
        size_t val = i; // Recreate the same unique key
        std::memcpy(key.data(), &val, std::min(sizeof(val), TEST_KEY_SIZE));
        BOOST_CHECK(ht.find(key) == nullptr);
    }

    ht.clear();
    BOOST_CHECK(ht.empty());
    BOOST_CHECK_EQUAL(ht.size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
