#pragma once

#include <chrono>
#include <cstdint>
#include <array>
#include <cstring>
#include <algorithm>

namespace hw::utility {

template <size_t SLOTS = 1024>
class OrderBurstControl {
    static_assert((SLOTS & (SLOTS - 1)) == 0, "SLOTS must be a power of 2");

public:
    using Timestamp = int64_t; // Nanoseconds

    enum class Mode { Normal, Cooldown };

    struct State {
        Mode mode;
        Timestamp start_time;
        size_t total_count;
    };

    /**
     * @param heatupWin Sliding window duration for normal mode.
     * @param heatupMaxCnt Max orders allowed in heatupWin.
     * @param cooldownWin Sliding window duration for cooldown mode.
     * @param cooldownMaxCnt Max orders allowed in cooldownWin to exit cooldown.
     */
    OrderBurstControl(std::chrono::milliseconds heatupWin, size_t heatupMaxCnt,
                      std::chrono::milliseconds cooldownWin, size_t cooldownMaxCnt)
    {
        _heatup.window_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(heatupWin).count();
        _heatup.max_cnt = heatupMaxCnt;
        _heatup.slot_width_ns = _heatup.window_ns / SLOTS;
        if (_heatup.slot_width_ns == 0) _heatup.slot_width_ns = 1;

        _cooldown.window_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(cooldownWin).count();
        _cooldown.max_cnt = cooldownMaxCnt;
        _cooldown.slot_width_ns = _cooldown.window_ns / SLOTS;
        if (_cooldown.slot_width_ns == 0) _cooldown.slot_width_ns = 1;

        // Initialize counters
        std::memset(_counters.data(), 0, sizeof(_counters));
        
        // Start in Normal mode
        _current_mode = Mode::Normal;
        _current_config = &_heatup;
    }

    /**
     * @return current state including mode, cooldown start time, and total counter.
     */
    State state() const {
        return State{
            _current_mode,
            (_current_mode == Mode::Normal) ? 0 : _cooldown_start_tm,
            _total_count
        };
    }

    /**
     * Evaluates whether an order is allowed.
     * @param tm Current system timestamp in nanoseconds (may not be monotonic across threads).
     * @return true if allowed, false if rejected (and potentially switched to cooldown).
     */
    bool evaluate(Timestamp tm) {
        // 1. Calculate current absolute slot index
        const size_t abs_slot = tm / _current_config->slot_width_ns;
        
        // Handle non-monotonic timestamps or lagging threads:
        // Case A: tm is in the past relative to _last_abs_slot
        if (abs_slot < _last_abs_slot) {
             size_t delta_back = _last_abs_slot - abs_slot;
             if (delta_back >= SLOTS) {
                 // Too old: exceeds window size. Ignore this event.
                 // "impossibility in practice" but handled safely.
                 return false; 
             }
             // Else: It falls into a valid past slot. We will increment it below.
             // We do NOT prune history because the "current" time (_last_abs_slot) hasn't advanced.
        } 
        // Case B: tm is in the future (or equal)
        else {
            // Lazy Pruning: Clear slots that were skipped since last update
            size_t delta = abs_slot - _last_abs_slot;
            
            if (delta >= SLOTS) {
                // We wrapped around the entire buffer. All history is gone.
                std::memset(_counters.data(), 0, sizeof(_counters));
                _total_count = 0;
            } else if (delta > 0) {
                // We skipped some slots. Clear them.
                for (size_t i = 1; i <= delta; ++i) {
                    size_t slot_idx = (_last_abs_slot + i) & (SLOTS - 1);
                    _total_count -= _counters[slot_idx];
                    _counters[slot_idx] = 0;
                }
            }
            // Update "head" of time
            _last_abs_slot = abs_slot;
        }

        // 3. Logic based on Mode
        if (_current_mode == Mode::Normal) {
            if (_total_count < _current_config->max_cnt) {
                // Allowed
                increment_(abs_slot);
                return true;
            } else {
                // Limit reached: Switch to Cooldown
                switch_mode_(Mode::Cooldown, tm);
                
                // Recalculate slot for new config
                size_t new_abs_slot = tm / _current_config->slot_width_ns;
                _last_abs_slot = new_abs_slot;
                increment_(new_abs_slot);
                return false;
            }
        } else {
            // Mode::Cooldown
            if ((tm - _cooldown_start_tm) >= _cooldown.window_ns) {
                 if (_total_count <= _cooldown.max_cnt) {
                     // Switch back to Normal
                     switch_mode_(Mode::Normal, tm);
                     
                     size_t new_abs_slot = tm / _current_config->slot_width_ns;
                     _last_abs_slot = new_abs_slot;
                     increment_(new_abs_slot);
                     return true; 
                 }
            }
            
            increment_(abs_slot);
            return false;
        }
    }

private:
    struct Config {
        int64_t window_ns;
        size_t max_cnt;
        int64_t slot_width_ns;
    };

    inline void increment_(size_t abs_slot) {
        size_t idx = abs_slot & (SLOTS - 1);
        _counters[idx]++;
        _total_count++;
    }

    inline void switch_mode_(Mode new_mode, Timestamp now) {
        _current_mode = new_mode;
        if (new_mode == Mode::Normal) {
            _current_config = &_heatup;
        } else {
            _current_config = &_cooldown;
            _cooldown_start_tm = now;
        }
        
        // Clear history
        std::memset(_counters.data(), 0, sizeof(_counters));
        _total_count = 0;
    }

    Config _heatup;
    Config _cooldown;

    Mode _current_mode;
    Config* _current_config;
    Timestamp _cooldown_start_tm = 0;

    // Counters
    std::array<size_t, SLOTS> _counters;
    size_t _total_count = 0;
    size_t _last_abs_slot = 0;
};

} // namespace hw::utility
