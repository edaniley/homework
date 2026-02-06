#include <boost/test/unit_test.hpp>
#include <hw/utility/OrderBurstControl.hpp>
#include <thread>
#include <vector>
#include <chrono>

using namespace hw::utility;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(OrderBurstControlTests)

// 1. Basic Functionality: Normal Mode -> Limit Reached -> Cooldown -> Back to Normal
BOOST_AUTO_TEST_CASE(test_basic_lifecycle) {
    // Heatup: 100ms window, max 5 orders
    // Cooldown: 100ms window, max 2 orders to exit
    // Slots: 16 (small for easier reasoning)
    using Control = OrderBurstControl<16>;
    Control obc(100ms, 5, 100ms, 2);
    int64_t now_ns = 1000000; // Start at arbitrary time

    // 1. Fill heatup capacity (5 orders)
    for (int i = 0; i < 5; ++i) {
        BOOST_CHECK_EQUAL(obc.evaluate(now_ns), true);
        now_ns += 1000; // +1us
    }
    BOOST_CHECK(obc.state().mode == Control::Mode::Normal);
    BOOST_CHECK_EQUAL(obc.state().total_count, 5);

    // 2. Trigger Cooldown (6th order)
    BOOST_CHECK_EQUAL(obc.evaluate(now_ns), false);
    BOOST_CHECK(obc.state().mode == Control::Mode::Cooldown);
    
    // 3. Stay in Cooldown (Verify rejection)
    now_ns += 50000000; // +50ms (Halfway through cooldown)
    BOOST_CHECK_EQUAL(obc.evaluate(now_ns), false); 
    
    // 4. Exit Cooldown
    // Condition: Time > cooldownWin (100ms) AND count <= cooldownMax (2)
    // We are at T+50ms. Let's move to T+110ms.
    // The previous calls were at T+0 (6 calls).
    // At T+110ms, the calls at T+0 are > 100ms old, so they should be pruned.
    // Window is empty -> count 0 <= 2. Should exit.
    now_ns += 60000000; // +60ms -> Total +110ms from start
    
    // This call checks exit condition logic
    // Pruning happens first. 
    BOOST_CHECK_EQUAL(obc.evaluate(now_ns), true); 
    BOOST_CHECK(obc.state().mode == Control::Mode::Normal);
    // Count should be 1 (the current accepted call)
    BOOST_CHECK_EQUAL(obc.state().total_count, 1);
}

// 2. Window Sliding & Pruning Logic
BOOST_AUTO_TEST_CASE(test_sliding_window) {
    using Control = OrderBurstControl<1024>; // Standard
    Control obc(100ms, 10, 100ms, 10);
    int64_t now_ns = 0;

    // Insert 10 items at T=0
    for(int i=0; i<10; ++i) BOOST_CHECK(obc.evaluate(now_ns));
    BOOST_CHECK_EQUAL(obc.state().total_count, 10);

    // Move to T=50ms (Half window). Nothing expires.
    now_ns += 50000000; 
    // This inserts 11th item -> should fail and go to cooldown
    BOOST_CHECK_EQUAL(obc.evaluate(now_ns), false);
    BOOST_CHECK(obc.state().mode == Control::Mode::Cooldown);

    // Reset test for clean sliding verification
    Control obc2(100ms, 10, 100ms, 10);
    now_ns = 0;
    // Insert 1 per 10ms. T=0, 10, 20... 90. (10 items)
    for(int i=0; i<10; ++i) {
        BOOST_CHECK(obc2.evaluate(now_ns));
        now_ns += 10000000; 
    }
    // now_ns is 100ms.
    // Window is [0, 100ms). Item at 0 is just expiring or about to.
    // Let's advance to 101ms.
    now_ns = 101000000;
    // Item at T=0 (0ms) should be pruned. Item at T=10ms is age 91ms (kept).
    // Total count should drop from 10 to 9, then +1 for current = 10.
    BOOST_CHECK(obc2.evaluate(now_ns)); 
    BOOST_CHECK_EQUAL(obc2.state().total_count, 10); 
    BOOST_CHECK(obc2.state().mode == Control::Mode::Normal);
}

// 3. Non-Monotonic / Out-of-Order Timestamps
BOOST_AUTO_TEST_CASE(test_out_of_order) {
    using Control = OrderBurstControl<64>;
    Control obc(100ms, 100, 100ms, 100);
    int64_t base_ns = 1000000000; // 1s

    // 1. Current event
    obc.evaluate(base_ns); 
    
    // 2. Future event (Advances window head)
    obc.evaluate(base_ns + 50000000); // +50ms
    BOOST_CHECK_EQUAL(obc.state().total_count, 2);

    // 3. Past event (Within window)
    // Should be counted but not move head
    obc.evaluate(base_ns + 25000000); // +25ms (middle)
    BOOST_CHECK_EQUAL(obc.state().total_count, 3);

    // 4. Very old event (Older than window)
    // Window head is at base + 50ms. Window size 100ms.
    // Valid range: [base-50ms, base+50ms].
    // Event at base - 60ms is too old.
    bool res = obc.evaluate(base_ns - 60000000); 
    BOOST_CHECK_EQUAL(res, false);
    BOOST_CHECK_EQUAL(obc.state().total_count, 3); // Should not change
}

// 4. Cooldown Extension (Failing to exit)
BOOST_AUTO_TEST_CASE(test_cooldown_extension) {
    using Control = OrderBurstControl<128>;
    // Heatup: Max 2. Cooldown: Max 1.
    Control obc(100ms, 2, 100ms, 1);
    int64_t now_ns = 1000;

    obc.evaluate(now_ns); // 1
    obc.evaluate(now_ns); // 2
    BOOST_CHECK_EQUAL(obc.evaluate(now_ns), false); // 3 -> Cooldown
    
    // In Cooldown. 
    // We keep spamming.
    for(int i=0; i<10; ++i) {
        now_ns += 10000000; // +10ms
        obc.evaluate(now_ns);
    }
    // Now at T=100ms approx.
    // Start of cooldown was T=1us.
    // Time passed ~100ms. 
    // Count is high (spamming). 
    
    now_ns += 10000000; // T=110ms. > 100ms window.
    // Logic: Time > Window. But Count (10+) > Max (1). 
    // Should STAY in cooldown.
    BOOST_CHECK_EQUAL(obc.evaluate(now_ns), false);
    BOOST_CHECK(obc.state().mode == Control::Mode::Cooldown);
    
    // Stop spamming. Wait for window to clear.
    now_ns += 150000000; // +150ms. Total silence.
    // History should prune. Count -> 0.
    // 0 <= 1. Should exit.
    BOOST_CHECK_EQUAL(obc.evaluate(now_ns), true);
    BOOST_CHECK(obc.state().mode == Control::Mode::Normal);
}

// 5. Wrap Around / Large Gaps
BOOST_AUTO_TEST_CASE(test_large_time_gaps) {
    using Control = OrderBurstControl<1024>;
    Control obc(100ms, 10, 100ms, 10);
    
    obc.evaluate(1000);
    BOOST_CHECK_EQUAL(obc.state().total_count, 1);
    
    // Jump forward 1 hour
    obc.evaluate(3600000000000LL); 
    // Should have cleared everything and count should be 1 (the new one)
    BOOST_CHECK_EQUAL(obc.state().total_count, 1);
}

// 6. Zero-sized window edge case handling (Constructors handles division by zero?)
// Config ensures slot width >= 1. 

BOOST_AUTO_TEST_SUITE_END()
