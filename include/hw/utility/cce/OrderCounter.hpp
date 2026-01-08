// --- START FILE: include/hw/utility/cce/OrderCounter.hpp ---

#pragma once

#include <cstdint>
#include <array>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <chrono>

#include <hw/utility/Time.hpp>


namespace hw::utility::cce {
  using namespace hw::utility;
  using Nanoseconds = int64_t;

// Per-parent order counter implementing burst control.
// Not thread-safe.
// BUCKETS: number of discrete slices in the rolling window.
template <size_t BUCKETS>
class OrderCounter {
public:
  static_assert(BUCKETS > 0, "BUCKETS must be > 0");
  static constexpr Nanoseconds MIN_WINDOW_NS  = static_cast<Nanoseconds>(1'000'000); // 1 ms
  static constexpr size_t MAX_LIMIT           = 10'000;

  template <typename Duration>
  requires IsDuration<Duration>
  OrderCounter(Duration window, size_t limit)
    : _limit(validateLimit_(limit))
    , _resolution(validateResolution_(validateWindow_(DurationToNanoseconds(window)), BUCKETS))
    , _lastTimestamp(0)
    , _totalValue(0)
  {
    _buckets.fill(0);
  }

  [[nodiscard]]
  inline bool increment (Nanoseconds timestamp) noexcept {
    rollWindow_(timestamp);

    if (_totalValue >= _limit) [[unlikely]] {
      return false;
    }

    const size_t idx = (static_cast<uint64_t>(timestamp) / _resolution) % BUCKETS;
    _buckets[idx] += 1;
    _totalValue   += 1;
    _lastTimestamp = timestamp;
    return true;
  }

  inline size_t	      value()         const noexcept { return _totalValue; }
  inline size_t	      limit()         const noexcept { return _limit; }
  inline Nanoseconds  resolution()    const noexcept { return _resolution; }
  inline Nanoseconds  lastTimestamp() const noexcept { return _lastTimestamp; }
  inline Nanoseconds  window() const noexcept {
    return _resolution * static_cast<Nanoseconds>(BUCKETS);
  }

private:
  static inline Nanoseconds validateWindow_(Nanoseconds window) {
    if (window < MIN_WINDOW_NS) {
      throw std::invalid_argument("OrderCounter: window must be at least 1 millisecond (>= 1'000'000 ns)");
    }
    return window;
  }

  static inline size_t validateLimit_(size_t limit) {
    if (limit == 0 || limit > MAX_LIMIT) {
      throw std::invalid_argument("OrderCounter: limit must be in range [1, 10'000]");
    }
    return limit;
  }

  static constexpr Nanoseconds validateResolution_(Nanoseconds window, size_t buckets) noexcept {
    const uint64_t w = static_cast<uint64_t>(window);
    const uint64_t b = static_cast<uint64_t>(buckets);
    const uint64_t res = (w + b - 1) / b; // ceil(w/b）
    return static_cast<Nanoseconds> (res == 0 ? 1 : res);
  }

  inline void rollWindow_(Nanoseconds timestamp) noexcept {
    assert (timestamp >= _lastTimestamp);

    if (timestamp < _lastTimestamp) [[unlikely]] {
      return;
    }

    const uint64_t currentTick  = static_cast<uint64_t>(timestamp) / _resolution;
    const uint64_t lastTick     = static_cast<uint64_t>(_lastTimestamp) / _resolution;
    const uint64_t diff = currentTick - lastTick;
    if (diff == 0) [[likely]] {
      _lastTimestamp = timestamp; // keep Last seen time in sync
      return;
    }

    if (diff >= BUCKETS) {
      _buckets.fill(0);
      _totalValue = 0;
    }
    else {
      // CLear only the aged-out slices: (lastTick + 1) .. (lastTick + diff)
      for (size_t i = 1; i <= diff; ++i) {
        const size_t clearIdx = (lastTick + i) % BUCKETS;
        _totalValue -= _buckets[clearIdx];
        _buckets[clearIdx] = 0;
      }
    }
    _lastTimestamp = timestamp;
  }

  const size_t	              _limit;
  const Nanoseconds	          _resolution;
  std::array<size_t, BUCKETS> _buckets;
  Nanoseconds                 _lastTimestamp;
  size_t                      _totalValue;
};

}

// --- START FILE: include/hw/utility/cce/OrderCounter.hpp ---
