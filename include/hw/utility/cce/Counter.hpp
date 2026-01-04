// --- START FILE: include/hw/utility/cce/Counter.hpp ---
#pragma once

#include <cstdint>
#include <array>
#include <algorithm>

#include <hw/utility/Time.hpp>

namespace hw::utility::cce {

/**
 * Counter: Per-parent order burst control.
 * BUCKETS: Number of discrete time slices in the rolling window.
 */
template <size_t BUCKETS>
class Counter {
    static_assert(BUCKETS > 0, "Counter must have at least one bucket");

public:
    /**
     * @param windowNs Total duration of the rolling window (e.g., 20ms)
     * @param limit Maximum allowed increments within the window
     */
    Counter(Timestamp windowNs, size_t limit)
        : _limit(limit)
        , _resolutionNs(windowNs / BUCKETS)
        , _lastTimestampNs(0)
        , _totalValue(0) {
        _buckets.fill(0);
    }

    template <typename Duration>
    Counter(Duration window, size_t limit)
        : Counter(DurationToNanoseconds(window), limit) {}

    /**
     * Hot-path increment:
     * Direct flow into window rolling and limit checking.
     */
    bool increment(Timestamp timestampNs) {
        rollWindow(timestampNs);

        if (_totalValue >= _limit) [[unlikely]] {
            return false;
        }

        const size_t idx = (static_cast<uint64_t>(timestampNs) / _resolutionNs) % BUCKETS;

        _buckets[idx]++;
        _totalValue++;
        _lastTimestampNs = timestampNs;

        return true;
    }

    size_t value() const { return _totalValue; }
    size_t limit() const { return _limit; }

private:
    /**
     * Optimized rollWindow:
     * Calculates tick deltas and clears aged buckets.
     */
    inline void rollWindow(Timestamp timestampNs) {
        const uint64_t currentTick = static_cast<uint64_t>(timestampNs) / _resolutionNs;
        const uint64_t lastTick = static_cast<uint64_t>(_lastTimestampNs) / _resolutionNs;

        // diff is the number of resolution-sized steps time has moved forward
        const uint64_t diff = currentTick - lastTick;

        if (diff == 0) [[likely]] {
            return;
        }

        if (diff >= BUCKETS) {
            _buckets.fill(0);
            _totalValue = 0;
        } else {
            // Clear only the buckets that have aged out since the last update
            for (size_t i = 1; i <= diff; ++i) {
                const size_t clearIdx = (lastTick + i) % BUCKETS;
                _totalValue -= _buckets[clearIdx];
                _buckets[clearIdx] = 0;
            }
        }
        _lastTimestampNs = timestampNs;
    }

    const size_t _limit;
    const Timestamp _resolutionNs;

    std::array<size_t, BUCKETS> _buckets;
    Timestamp _lastTimestampNs;
    size_t _totalValue;
};

} // namespace hw::utility::cce