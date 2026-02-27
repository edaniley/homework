#pragma once

#include <atomic>

namespace hw::utility {

// A simple spinlock implementation using std::atomic_flag
class Spinlock {
public:
    Spinlock() = default;

    // Delete copy and move operations to ensure single ownership
    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;
    Spinlock(Spinlock&&) = delete;
    Spinlock& operator=(Spinlock&&) = delete;

    // Locks the spinlock
    void lock() noexcept {
        // Atomically set the flag and check its previous value
        // Loop while the flag is set (meaning it's locked by another thread)
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Use processor-specific hint for busy-waiting, if available
            // This helps reduce power consumption and improve performance in some cases
            #ifdef __GNUC__
                __builtin_ia32_pause(); // For x86/x64 architectures
            #endif
        }
    }

    // Unlocks the spinlock
    void unlock() noexcept {
        // Atomically clear the flag
        flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT; // Initialize flag to clear (unlocked) state
};

} // namespace hw::utility
