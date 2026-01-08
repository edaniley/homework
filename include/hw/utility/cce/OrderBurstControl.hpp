// --- START FILE: include/hw/utility/cce/OrderBurstControl.hpp ---
#pragma once

#include <cstdint>
#include <hw/utility/Time.hpp>
#include <hw/utility/Allocator.hpp>
#include <hw/utility/cce/OrderCounter.hpp>
#include <hw/utility/cce/HashTable.hpp>

namespace hw::utility::cce {

/**
 * OrderBurstControl: Orchestrates per-parent order rate limiting.
 * * - Decoupled from Clock: Timestamps must be provided by the caller.
 * - Thread-Safe via TLS: HashMaps and Allocators are thread-local.
 * - Zero-Heap: All counters are pre-allocated in a thread-local pool.
 */
template <size_t BUCKETS = 20, size_t MAX_PARENTS = 1024>
class OrderBurstControl {
public:
    /**
     * @param window Time window for rate limiting (e.g., 20ms).
     * @param limit Maximum messages allowed per parent within that window.
     */
    template <typename Duration>
    OrderBurstControl(Duration window, size_t limit)
        : _windowNs(DurationToNanoseconds(window))
        , _limit(limit) {}

    /**
     * Registers a new parent order in the calling thread's local storage.
     */
    void addParent(uint64_t parentOrderID) {
        auto& state = get_state();

        // Ensure we don't double-register and leak a counter from the pool
        if (state.map.find(parentOrderID)) [[unlikely]] return;

        auto* counter = state.allocator.allocate(_windowNs, _limit);
        if (counter) [[likely]] {
            state.map.insert(parentOrderID, counter);
        }
    }

    /**
     * Removes the parent and recycles the OrderCounter memory to the pool.
     */
    void removeParent(uint64_t parentOrderID) {
        auto& state = get_state();
        auto* counter = state.map.find(parentOrderID);

        if (counter) {
            state.map.erase(parentOrderID);
            state.allocator.free(counter);
        }
    }

    /**
     * Hot-path: Check if a child order is allowed.
     * @param parentOrderID The ID of the parent to check.
     * @param nowNs Current timestamp in nanoseconds (e.g., from SystemClockTSC).
     * @return true if allowed, false if throttled.
     */
    inline bool addChild(uint64_t parentOrderID, Timestamp nowNs) {
        auto& state = get_state();
        auto* counter = state.map.find(parentOrderID);

        if (counter == nullptr) [[unlikely]] {
            return false;
        }

        return counter->increment(nowNs);
    }

    /**
     * Monitoring: Returns current message count for a parent.
     */
    size_t childCount(uint64_t parentOrderID) const {
        auto* counter = get_state().map.find(parentOrderID);
        return counter ? counter->value() : 0;
    }

    /**
     * Monitoring: Returns number of active parents in this thread.
     */
    size_t parentCount() const {
        return get_state().map.size();
    }

private:
    struct ThreadState {
        SwissTableHashmap<OrderCounter<BUCKETS>, MAX_PARENTS> map;
        AllocatorTrivial<OrderCounter<BUCKETS>> allocator;

        ThreadState(size_t capacity) : allocator(capacity) {}
    };

    inline ThreadState& get_state() const {
        static thread_local ThreadState state(MAX_PARENTS);
        return state;
    }

    const Timestamp _windowNs;
    const size_t _limit;
};

} // namespace hw::utility::cce