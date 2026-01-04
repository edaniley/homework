#include <boost/test/unit_test.hpp>
#include <hw/utility/cce/Counter.hpp>
#include <thread>

BOOST_AUTO_TEST_CASE(test_counter_burst_limit) {
    using namespace hw::utility;
    using namespace hw::utility::cce;

    // 20ms window, 20 buckets (1ms resolution), limit 3
    Counter<20> counter(std::chrono::milliseconds(20), 3);

    Timestamp now = 1000000000; // 1s in nanos

    BOOST_CHECK(counter.increment(now));
    BOOST_CHECK(counter.increment(now + 100)); // Same bucket
    BOOST_CHECK(counter.increment(now + 200));

    // 4th increment in same window should fail
    BOOST_CHECK(!counter.increment(now + 300));

    // Move time forward 25ms (past the 20ms window)
    BOOST_CHECK(counter.increment(now + 25000000));
}
