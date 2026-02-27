#include <boost/test/unit_test.hpp>
#include <hw/utility/Allocator.hpp>

struct MockOrder {
    uint64_t id;
    double price;
    char side;
};

BOOST_AUTO_TEST_CASE(test_allocator_trivial_basic) {
    using namespace hw::utility;
    AllocatorTrivial<MockOrder> allocator(10);

    MockOrder* o1_mem = allocator.allocate();
    allocator.construct(o1_mem, 1ULL, 100.5, 'B');
    MockOrder* o1 = o1_mem;
    BOOST_REQUIRE(o1 != nullptr);
    BOOST_CHECK_EQUAL(o1->id, 1ULL);

    allocator.destroy(o1);
    allocator.free(o1);

    // Test recycling: next allocation should reuse the same pointer
    MockOrder* o2_mem = allocator.allocate();
    allocator.construct(o2_mem, 2ULL, 101.0, 'S');
    MockOrder* o2 = o2_mem;
    BOOST_CHECK(o1 == o2);
    BOOST_CHECK_EQUAL(o2->id, 2ULL);

    allocator.destroy(o2);
    allocator.free(o2);
}
