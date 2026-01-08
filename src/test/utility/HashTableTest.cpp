#include <boost/test/unit_test.hpp>
#include <hw/utility/cce/HashTable.hpp>
#include <vector>
#include <numeric>

using namespace hw::utility::cce;

BOOST_AUTO_TEST_SUITE(SwissTableTests)

// 1. Basic Functionality
BOOST_AUTO_TEST_CASE(test_basic_insert_find) {
    SwissTableHashmap<int, 32> map;
    int val1 = 100, val2 = 200;

    BOOST_CHECK(map.insert(10, &val1));
    BOOST_CHECK(map.insert(20, &val2));
    BOOST_CHECK_EQUAL(map.size(), 2);

    BOOST_CHECK_EQUAL(map.find(10), &val1);
    BOOST_CHECK_EQUAL(map.find(20), &val2);
    BOOST_CHECK(map.find(30) == nullptr);
}

// 2. SIMD Boundary & Mirroring Test
// Tests if keys that hash to the very end of the array are found via the mirrored tail
BOOST_AUTO_TEST_CASE(test_simd_boundary_wrap) {
    // We use a small power-of-two size
    SwissTableHashmap<int, 16> map;
    int val = 999;

    // We want to force an index near the end.
    // Since idx = (hash >> 7) & (SLOTS - 1), we probe keys until we hit index 15.
    uint64_t boundary_key = 0;
    for (uint64_t k = 0; k < 1000; ++k) {
        size_t idx = (SwissTableHashmap<int, 16>::hash_(k) >> 7) & 15;
        if (idx == 15) {
            boundary_key = k;
            break;
        }
    }

    map.insert(boundary_key, &val);

    // The SIMD load starting at index 15 will read 1 byte from index 15
    // and 15 bytes from the mirrored tail [_ctrl[16]..._ctrl[30]].
    BOOST_CHECK_EQUAL(map.find(boundary_key), &val);
}

// 3. Collision and Linear Probing
BOOST_AUTO_TEST_CASE(test_collisions) {
    SwissTableHashmap<int, 64> map;
    int v1 = 1, v2 = 2, v3 = 3;

    // Find three keys that map to the same starting index
    std::vector<uint64_t> keys;
    size_t target_idx = 5;
    for (uint64_t k = 0; keys.size() < 3; ++k) {
        if (((SwissTableHashmap<int, 64>::hash_(k) >> 7) & 63) == target_idx) {
            keys.push_back(k);
        }
    }

    map.insert(keys[0], &v1);
    map.insert(keys[1], &v2);
    map.insert(keys[2], &v3);

    BOOST_CHECK_EQUAL(map.find(keys[0]), &v1);
    BOOST_CHECK_EQUAL(map.find(keys[1]), &v2);
    BOOST_CHECK_EQUAL(map.find(keys[2]), &v3);
}

// 4. Erase and Slot Reclamation
BOOST_AUTO_TEST_CASE(test_erase_reclamation) {
    SwissTableHashmap<int, 16> map;
    int v = 42;
    uint64_t key = 12345;

    map.insert(key, &v);
    BOOST_CHECK_EQUAL(map.size(), 1);

    map.erase(key);
    BOOST_CHECK_EQUAL(map.size(), 0);
    BOOST_CHECK(map.find(key) == nullptr);

    // Re-inserting should reclaim the 'Deleted' slot
    map.insert(key, &v);
    BOOST_CHECK_EQUAL(map.size(), 1);
    BOOST_CHECK_EQUAL(map.find(key), &v);
}

// 5. Table Full Condition
BOOST_AUTO_TEST_CASE(test_table_full) {
    SwissTableHashmap<int, 16> map;
    int vals[16];

    for (uint64_t i = 0; i < 16; ++i) {
        BOOST_CHECK(map.insert(i, &vals[i]));
    }

    // 17th insert must fail
    int extra = 100;
    BOOST_CHECK_EQUAL(map.insert(99, &extra), false);
    BOOST_CHECK_EQUAL(map.size(), 16);
}

// 6. Update Existing Key
BOOST_AUTO_TEST_CASE(test_update_value) {
    SwissTableHashmap<int, 16> map;
    int v1 = 10, v2 = 20;
    uint64_t key = 55;

    map.insert(key, &v1);
    BOOST_CHECK_EQUAL(map.find(key), &v1);

    // Update
    map.insert(key, &v2);
    BOOST_CHECK_EQUAL(map.find(key), &v2);
    BOOST_CHECK_EQUAL(map.size(), 1); // Size should not increase
}

// 7. Clear Test
BOOST_AUTO_TEST_CASE(test_clear) {
    SwissTableHashmap<int, 16> map;
    int v = 1;
    map.insert(1, &v);
    map.insert(2, &v);

    map.clear();
    BOOST_CHECK_EQUAL(map.size(), 0);
    BOOST_CHECK(map.find(1) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()