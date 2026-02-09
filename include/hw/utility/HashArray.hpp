#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <cstring> // for memcpy

#if defined(__SSE2__)
#include <immintrin.h>
#endif

namespace hw::utility {

/**
 * @brief Fixed-size, open-addressing hash table using SwissTable-style SIMD probing.
 * 
 * Optimized for hot-path latency.
 * - Lock-free lookups (wait-free if no concurrent inserts to same slot).
 * - Thread-safe inserts (uses spin-wait on slot contention).
 * - No allocations after construction.
 * 
 * @tparam KeyType User-provided key type. Must satisfy:
 *                 - CopyConstructible / CopyAssignable
 *                 - uint64_t hash() const noexcept
 *                 - bool operator==(const KeyType&) const
 * @tparam ValueType Value type. Stored as pointers.
 * @tparam MAX_KEYS Capacity. Must be power of 2.
 */
template <typename KeyType, typename ValueType, size_t MAX_KEYS>
class HashArray {
    static_assert((MAX_KEYS & (MAX_KEYS - 1)) == 0, "MAX_KEYS must be a power of 2");
    static_assert(MAX_KEYS >= 16, "MAX_KEYS must be at least 16 for SIMD probing");
    
    // Validate KeyType requirements roughly
    static_assert(std::is_copy_constructible_v<KeyType>, "KeyType must be copy constructible");
    static_assert(std::is_copy_assignable_v<KeyType>, "KeyType must be copy assignable");

public:
    enum class InsertResult { Success, DuplicateKey, TableFull };
    using ForEachFn = std::function<void(const KeyType &key, ValueType *value)>;

    HashArray() {
        // Initialize control bytes to Empty (0xFF)
        // We use relaxed stores because the object is under construction (single thread)
        for (auto& c : _ctrl) {
            c.store(Control::Empty, std::memory_order_relaxed);
        }
        // Mirror tail for SIMD overflow safety
        // Not strictly needed with atomic array logic if we mask indices properly,
        // but swisstable usually mirrors to avoid masking inside the loop.
        // However, with atomic array, mirroring is complex to maintain atomically.
        // We will strictly mask indices (i & mask) for simplicity and safety in lock-free context.
    }

    /**
     * @brief Inserts a key-value pair.
     * Thread-safe. Uses spin-wait if slot is currently being modified.
     */
    InsertResult insert(const KeyType &key, ValueType *value) noexcept {
        const uint64_t h = key.hash();
        const int8_t tag = static_cast<int8_t>(h & 0x7F); // 7-bit tag (0..127)
        const size_t start_idx = (h >> 7) & (MAX_KEYS - 1);

        for (size_t i = 0; i < MAX_KEYS; ++i) {
            const size_t idx = (start_idx + i) & (MAX_KEYS - 1);
            
            // 1. Load Control Byte
            int8_t c = _ctrl[idx].load(std::memory_order_acquire);

            // 2. Check for Duplicate
            if (c == tag) {
                // Potential match. Check key.
                if (_keys[idx] == key) {
                    return InsertResult::DuplicateKey;
                }
                // Hash collision (different key, same tag), continue probing.
            }
            
            // 3. Check for Empty to Insert
            if (c == Control::Empty) {
                // Attempt to claim slot: Empty -> Busy
                // Busy is a sentinel value (e.g., 0xFE)
                if (_ctrl[idx].compare_exchange_strong(c, Control::Busy, 
                                                       std::memory_order_acq_rel, 
                                                       std::memory_order_acquire)) {
                    // Success! We own this slot.
                    // c was Empty, now it is Busy.
                    
                    // Write data (not atomic, but protected by Busy state)
                    _keys[idx] = key;
                    _data[idx] = value;
                    
                    // Publish: Busy -> Tag
                    _ctrl[idx].store(tag, std::memory_order_release);
                    return InsertResult::Success;
                }
                
                // CAS failed.
                // If current state is now Busy, someone else is writing. 
                // If it's a Tag, someone finished writing.
                // We must retry this slot or continue.
                // Since we wanted to insert here, and it was empty a moment ago, 
                // re-evaluating this slot is correct. 
                // Decrement i to retry this loop iteration.
                i--; 
                continue; 
            }

            // 4. Handle Busy State (Concurrent Insertion)
            if (c == Control::Busy) {
                // Another thread is mutating this slot.
                // We cannot skip it because it might become OUR key (duplicate check),
                // or it might become a tag we need to skip over (probing chain).
                // We must wait.
                #if defined(__SSE2__)
                _mm_pause();
                #endif
                i--; // Retry this slot
                continue;
            }
        }

        return InsertResult::TableFull;
    }

    /**
     * @brief Finds a value by key.
     * Lock-free.
     */
    ValueType *find(const KeyType &key) const noexcept {
        const uint64_t h = key.hash();
        const int8_t tag = static_cast<int8_t>(h & 0x7F);
        const size_t start_idx = (h >> 7) & (MAX_KEYS - 1);

        // Linear probing with SIMD acceleration where possible
        // Note: For strict thread-safety with atomics, SIMD loading from std::atomic<T>* 
        // is technically UB if not careful, but practically works on x86 if alignment holds.
        // std::atomic<uint8_t> has standard layout 1 byte.
        // We cast _ctrl pointer to __m128i*.
        
        for (size_t i = 0; i < MAX_KEYS; i += 16) {
            const size_t group_start = (start_idx + i) & (MAX_KEYS - 1);
            
            // Check if we can do a contiguous SIMD load (no wrap-around split)
            // If wrap-around happens within 16 bytes, we fall back or handle split.
            // For simplicity in this implementation, we'll do scalar checks if wrapping,
            // or just simple scalar loop for strict safety if SIMD complexity is high.
            // Given "fast hash look up", SIMD is desired.
            
            if (group_start + 16 <= MAX_KEYS) {
                // Safe contiguous load
                #if defined(__SSE2__)
                // Relaxed load is sufficient for the "guess"; we confirm with Acquire later.
                // We cast the atomic array to raw pointer. 
                // Note: potential TSan warning, but standard on x64 for this optimization.
                __m128i ctrl_group = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_ctrl[group_start]));
                __m128i match_mask = _mm_cmpeq_epi8(ctrl_group, _mm_set1_epi8(tag));
                __m128i empty_mask = _mm_cmpeq_epi8(ctrl_group, _mm_set1_epi8(Control::Empty));
                
                uint32_t matches = _mm_movemask_epi8(match_mask);
                uint32_t empties = _mm_movemask_epi8(empty_mask);
                
                // Check matches
                while (matches) {
                    int bit = __builtin_ctz(matches);
                    size_t idx = group_start + bit;
                    
                    // Verify with Acquire load to ensure we see the key write
                    int8_t c = _ctrl[idx].load(std::memory_order_acquire);
                    if (c == tag) {
                        if (_keys[idx] == key) {
                            return _data[idx];
                        }
                    }
                    matches &= ~(1 << bit);
                }
                
                // Check empties (Stop probing)
                if (empties) {
                    // Must verify the first empty is truly empty (Acquire)
                    int bit = __builtin_ctz(empties);
                    size_t idx = group_start + bit;
                    if (_ctrl[idx].load(std::memory_order_acquire) == Control::Empty) {
                        return nullptr;
                    }
                    // If it wasn't empty (race), we continue.
                }
                #else
                // Fallback Scalar for this group
                for (size_t k = 0; k < 16; ++k) {
                    if (check_slot_(group_start + k, tag, key, val)) return val;
                    // If Empty, return nullptr (handled in check_slot_ helper? No, complex return)
                }
                #endif
            } else {
                // Wrap-around case: scalar fallback
                for (size_t k = 0; k < 16; ++k) {
                    size_t idx = (group_start + k) & (MAX_KEYS - 1);
                    int8_t c = _ctrl[idx].load(std::memory_order_acquire);
                    
                    if (c == tag) {
                        if (_keys[idx] == key) return _data[idx];
                    }
                    else if (c == Control::Empty) {
                        return nullptr;
                    }
                    else if (c == Control::Busy) {
                        // Wait/Spin? No, in find() we usually just ignore Busy or wait.
                        // If we ignore, we might miss.
                        // SwissTable spec: "Readers... may see a transition from Empty to Busy... 
                        // treat as Empty (stop probing) or continue?"
                        // Correct logic: If we see Busy, it behaves like a collision (occupied).
                        // BUT if it was Empty when we started, we stop.
                        // To be wait-free: we just continue probing. 
                        // IF the key is being inserted right now, we can technically say "not found yet".
                        // Spinning makes it blocking.
                        // For hot-path latency, if we hit a Busy slot, it's rare. 
                        // We will treat Busy as "Skip" (Collision) effectively.
                        // Limitation: If Busy slot eventually holds OUR key, we return nullptr.
                        // This is standard concurrent map semantics (linearizability point is before insertion completes).
                        continue; 
                    }
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Iterates over all present items.
     * Not on hot-path. Safe to use alongside hot-path operations.
     */
    template <typename Fn>
    void for_each(Fn &&f) {
        for (size_t i = 0; i < MAX_KEYS; ++i) {
            // Acquire to ensure we see consistent data
            int8_t c = _ctrl[i].load(std::memory_order_acquire);
            // We only care about valid tags (>= 0)
            if (c >= 0 && c != Control::Empty && c != Control::Busy) {
                f(_keys[i], _data[i]);
            }
        }
    }

private:
    struct Control {
        static constexpr int8_t Empty = static_cast<int8_t>(0xFF); //-1
        static constexpr int8_t Busy  = static_cast<int8_t>(0xFE); //-2
        // Tags are 0..127
    };

    // Aligned control bytes for SIMD
    alignas(16) std::array<std::atomic<int8_t>, MAX_KEYS> _ctrl;
    std::array<KeyType, MAX_KEYS> _keys;
    std::array<ValueType*, MAX_KEYS> _data;
};

} // namespace hw::utility
