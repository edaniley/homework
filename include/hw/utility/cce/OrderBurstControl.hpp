// --- START FILE: include/hw/utility/cce/OrderBurstControl.hpp ---
#pragma once

#include <cstdint>
#include <hw/utility/Time.hpp>
#include <hw/utility/Clock.hpp>
#include <hw/utility/Allocator.hpp>
#include <hw/utility/cce/Counter.hpp>
#include <hw/utility/cce/FastHashTable.hpp>

namespace hw::utility::cce {

/**
 * OrderBurstControl: Enforces per-parent order message limits using TLS.
 * Uses a shared Global Clock for consistent timestamping across threads.
 */
template <size_t BUCKETS = 20, size_t MAX_PARENTS = 1024>
class OrderBurstControl {
public:
    /**
     * @param clock Reference to the global calibrated SystemClockTSC
     */
    template <typename Duration>
    OrderBurstControl(const SystemClockTSC& clock, Duration window, size_t limit)
        : _globalClock(clock)
        , _windowNs(DurationToNanoseconds(window))
        , _limit(limit) {}

    /**
     * Registers a new parent order and initializes its burst counter.
     */
    void addParent(uint64_t parentOrderID) {
        auto& state = get_state();
        if (state.map.find(parentOrderID)) [[unlikely]] return;

        auto* counter = state.allocator.allocate(_windowNs, _limit);
        if (counter) [[likely]] {
            state.map.insert(parentOrderID, counter);
            state.activeParents++;
        }
    }

    /**
     * Removes the parent and recycles the counter memory.
     */
    void removeParent(uint64_t parentOrderID) {
        auto& state = get_state();
        auto* counter = state.map.find(parentOrderID);

        if (counter) {
            state.map.erase(parentOrderID);
            state.allocator.free(counter);
            if (state.activeParents > 0) state.activeParents--;
        }
    }

    /**
     * Hot-path: Check if a child order is allowed for this parent.
     */
    inline bool addChild(uint64_t parentOrderID) {
        auto& state = get_state();
        auto* counter = state.map.find(parentOrderID);

        if (counter == nullptr) [[unlikely]] {
            return false;
        }

        // Use the SHARED global clock
        return counter->increment(_globalClock.now());
    }

    size_t childCount(uint64_t parentOrderID) {
        auto* counter = get_state().map.find(parentOrderID);
        return counter ? counter->value() : 0;
    }

    size_t parentCount() {
        return get_state().activeParents;
    }

private:
    struct ThreadState {
        FastSIMDMap<Counter<BUCKETS>, MAX_PARENTS> map;
        AllocatorTrivial<Counter<BUCKETS>> allocator;
        size_t activeParents = 0;

        ThreadState(size_t capacity) : allocator(capacity) {}
    };

    inline ThreadState& get_state() const {
        static thread_local ThreadState state(MAX_PARENTS);
        return state;
    }

    const SystemClockTSC& _globalClock; // Reference to the singleton/global clock
    const Timestamp _windowNs;
    const size_t _limit;
};

} // namespace hw::utility::cce