#include <boost/test/unit_test.hpp>
#include <hw/utility/cce/FastHashTable.hpp>

BOOST_AUTO_TEST_CASE(test_fast_hash_table_simd) {
    using namespace hw::utility::cce;

    // Capacity 16 (minimum for our SIMD implementation)
    FastSIMDMap<int, 16> map;
    int val1 = 100;
    int val2 = 200;

    map.insert(42ULL, &val1);
    map.insert(99ULL, &val2);

    BOOST_CHECK_EQUAL(map.find(42ULL), &val1);
    BOOST_CHECK_EQUAL(map.find(99ULL), &val2);
    BOOST_CHECK(map.find(7ULL) == nullptr);

    map.erase(42ULL);
    BOOST_CHECK(map.find(42ULL) == nullptr);
}
