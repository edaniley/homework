// --- START FILE: include/hw/utility/cce/Map.hpp ---
#pragma once

#include <cstdint>
#include <array>
#include <immintrin.h>

namespace hw::utility::cce {

/**
 * FastSIMDMap: Fixed-capacity map optimized for hot-path lookup.
 * - Uses SIMD (SSE2) to probe 16 metadata slots in a single cycle.
 * - Value is intended to be Counter* allocated via AllocatorTrivial.
 */
template <typename Value, size_t SLOTS>
class FastSIMDMap {
    static_assert(!(SLOTS & (SLOTS - 1)), "SLOTS must be a power of 2");
    static_assert(SLOTS >= 16, "SLOTS must be at least 16 for SIMD probing");

    // Control bytes: 0xFF = Empty, 0x80 = Deleted, 0x00-0x7F = H2 hash tag
    enum Control : int8_t { Empty = -1, Deleted = -2 };

public:
    FastSIMDMap() {
        _metadata.fill(Control::Empty);
        _keys.fill(0);
        _values.fill(nullptr);
    }

    /**
     * Hot-path Find: Returns Value* or nullptr.
     * Guaranteed O(1) in the average case with extremely low constant factor.
     */
    inline Value* find(uint64_t key) const noexcept {
        const uint64_t h = hash(key);
        const int8_t tag = static_cast<int8_t>(h & 0x7F);       // H2: 7-bit tag
        size_t idx = (h >> 7) & (SLOTS - 1);                   // H1: starting slot

        // We use a fixed number of probes (standard is usually capacity)
        for (size_t i = 0; i < SLOTS; i += 16) {
            const size_t pos = (idx + i) & (SLOTS - 1);

            // Load 16 control bytes into a 128-bit register
            __m128i group = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_metadata[pos]));
            __m128i target = _mm_set1_epi8(tag);

            // Compare all 16 bytes for equality with the tag
            uint32_t match_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(group, target));

            // Check matches within this 16-slot group
            while (match_mask > 0) {
                int bit = __builtin_ctz(match_mask);
                size_t entry_idx = (pos + bit) & (SLOTS - 1);
                if (_keys[entry_idx] == key) [[likely]] {
                    return _values[entry_idx];
                }
                match_mask &= (match_mask - 1); // Clear the bit and check next match
            }

            // Optimization: if any slot in the group is 'Empty', the key does not exist
            uint32_t empty_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(group, _mm_set1_epi8(Control::Empty)));
            if (empty_mask > 0) [[likely]] {
                return nullptr;
            }
        }
        return nullptr;
    }

    /**
     * Insert: Associates key with value. Returns false if table is full.
     */
    inline bool insert(uint64_t key, Value* val) noexcept {
        const uint64_t h = hash(key);
        const int8_t tag = static_cast<int8_t>(h & 0x7F);
        size_t idx = (h >> 7) & (SLOTS - 1);

        for (size_t i = 0; i < SLOTS; ++i) {
            size_t pos = (idx + i) & (SLOTS - 1);
            if (_metadata[pos] < 0) { // Empty or Deleted
                _metadata[pos] = tag;
                _keys[pos] = key;
                _values[pos] = val;
                return true;
            }
            if (_keys[pos] == key) {
                _values[pos] = val; // Update existing
                return true;
            }
        }
        return false;
    }

    inline void erase(uint64_t key) noexcept {
        const uint64_t h = hash(key);
        const int8_t tag = static_cast<int8_t>(h & 0x7F);
        size_t idx = (h >> 7) & (SLOTS - 1);

        for (size_t i = 0; i < SLOTS; ++i) {
            size_t pos = (idx + i) & (SLOTS - 1);
            if (_metadata[pos] == Control::Empty) return;
            if (_metadata[pos] == tag && _keys[pos] == key) {
                _metadata[pos] = Control::Deleted;
                return;
            }
        }
    }

private:
    /**
     * MurmurHash3-style finalizer for uint64_t.
     * Essential for distributing OrderIDs evenly across slots.
     */
    static inline uint64_t hash(uint64_t x) noexcept {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return x;
    }

    // Aligned metadata ensures the SIMD loads are efficient
    alignas(16) std::array<int8_t, SLOTS> _metadata;
    std::array<uint64_t, SLOTS> _keys;
    std::array<Value*, SLOTS> _values;
};

} // namespace hw::utility::cce
// --- END FILE: include/hw/utility/cce/Map.hpp ---