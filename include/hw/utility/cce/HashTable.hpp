// --- START FILE: include/hw/utility/cce/HashTable.hpp ---

#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#if defined(__SSE2__)
#include <immintrin.h>
#endif
#ifdef DEBUG
#include <iostream>
#include < bitset>
#endif
namespace hw::utility::cce {
//
// https://abseil.io/about/design/swisstables
// SwissTableHashmap: fixed-capacity hash map (integer key -> pointer payload)
// - No allocations after construction
// - Open addressing with linear probing
// - SIMD-accelerated group probing of 16 control bytes (SSE2), with scalar fallback
// - Control bytes: OxFF = Empty, 0x80 = Deleted, 0x00.•0x7F = 7-bit tag
//
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#techs=SSE_ALL

template <typename Value, size_t SLOTS>
class SwissTableHashmap {

  static_assert (!(SLOTS & (SLOTS - 1)));
  static_assert(SLOTS >= 16);
  static constexpr size_t SIMD_SIZE = 16; // SIMD group width (SSE2)

  enum Control : int8_t {
    // must be negative values
    Empty   = static_cast<int8_t>(0xFF), // -1
    Deleted = static_cast<int8_t>(0x80) // -128
  };

#if defined(__SSE2__) && defined(DEBUG)
  static void print( m128i value) {
    std::bitset<sizeof(value)*8> bs;
    std::memcpy ((void *)&bs, &value, sizeof(value));
    std::cout << bs.to_string() << std::endl;
  }
#endif

public:
  SwissTableHashmap() : _size(0)//, _deleted (0)
  {
    // initialize control to Empty, and mirror first SIMD_SIZE bytes at the tail
    _ctrl.fill(Control::Empty);
    _keys.fill(0);
    _values.fill(nullptr);
    mirror_tail_();
  }



  // returns payload pointer or nullptr
  inline Value * find(uint64_t key) const noexcept {
    const uint64_t h = hash_(key);
    const int8_t tag = static_cast<int8_t>(h & 0x7F); // tag is first 7 bits
    const size_t idx = (h >> 7) & (SLOTS - 1);        // starting slot

#if defined(__SSE2__)
  // select and probe group starting with idx
  for (size_t i = 0; i < SLOTS; i += SIMD_SIZE) {
    const size_t j = (idx + i) & (SLOTS - 1);
    const __m128i group = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_ctrl[j]));
    const __m128i target = _mm_set1_epi8(tag); // 16 bytes , each set to tag

    //_mm_cmpeq_epi8 intrinsic performs an element-wise comparison for equality on two 128-bit SIMD registers,
    //    treating them as vectors of 16 signed or unsigned 8-bit integers (bytes).
    // Input: It takes two arguments, both of type _ml28i, which are 128-bit integer vectors stored in XMM registers.
    // Oeration: It compares each of the 16 bytes in the first input vector with the corresponding byte in the secon
    //    input vector simultaneously (in parallel).
    // Output: It returns a new __m128i vector of 16 bytes. For each element:
    //    If the corresponding bytes in the input vectors are equal, the resulting byte is set to OxFF (all bits are 1).
    //    If the corresponding bytes are not equal, the resulting byte is set to 0x00 (all bits are 0).
    const __m128i matchedBytes =_mm_cmpeq_epi8(group, target);
    //_mm movemask_epi8 intrinsic is an SSE2 instruction that takes a 128-bit integer vector
    //    containing 16 separate 8-bit elements and produces a 16-bit integer (an int) where each bit is
    //    the most significant bit (MSB) of the corresponding byte in the input vector.
    // The function extracts the most significant bit (the sign bit) from each of the 16 bytes in
    //    the input __m128i register and packs these bits into a single 16-bit integer result.
    // Input: A 128-bit SIMD register (e. g•,ml28) that holds 16 individual 8-bit values (bytes).
    // Process: For each of the 16 bytes, the intrinsic checks the 8th bit (the most significant bit).
    // Output: An int (typically 32-bit on x86-64 platforms, but only the lower 16 bits are used) where:
    //    Bit 0 of the result is the MSB of the first (least significant) byte of the input vector.
    //    Bit 1 of the result is the MSB of the second byte.
    //    ...and so on, up to bit 15, which is the MSB of the sixteenth (most significant) byte.
    const auto matchedBits = _mm_movemask_epi8(matchedBytes);
    // print (matchedBytes);
    // print (matchedBits);
    uint32_t matchMask = static_cast<uint32_t>(matchedBits);
    while (matchMask) {
      const int bit = __builtin_ctz(matchMask); // returns 1st set bit i.e. index of potential match
      const size_t entry_idx = (j + static_cast<size_t>(bit)) & (SLOTS - 1);
      if (_keys[entry_idx] == key) [[likely]] {
        return _values [entry_idx];
      }
      matchMask &= (matchMask - 1); // clears lowest set bit; try next if present
    }

    // early exit if nay byte in the group is Empty
    const uint32_t emptyMask = static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(group, _mm_set1_epi8 (Control:: Empty))));
    if (emptyMask) [[likely]] return nullptr;
  }
  return nullptr;
#else
  for (size_t i = 0; i < SLOTS; ++i) {
    const size_t pos = (idx + i) & (SLOTS - 1);
    const int8_t c = _ctrl[pos];
    if (c == tag && _keys [pos] == key) return _values[pos];
    if (c == Control:: Empty) return nullptr; // stop on empty
  }
  return nullptr;
#endif
}

//insert or update; returns false if table is full
inline bool insert(uint64_t key, Value* value) noexcept {
  const uint64_t hashval = hash_(key);
  const int8_t tag = static_cast<int8_t>(hashval & 0x7F);
  const size_t idx = (hashval >> 7) & (SLOTS - 1);
  for (size_t i = 0; i < SLOTS; ++i) {
    const size_t pos = (idx + i) & (SLOTS - 1);
    const int8_t ctrl = _ctrl[pos];

    // Empty or Deleted: claim slot
    if (ctrl < 0) {
      // if (ctrl == Control::Deleted) {// reclaim deleted
      //   if (_deleted) --_deleted;
      // }
      set_ctrl_(pos, tag);
      _keys[pos]   = key;
      _values[pos] = value;
      ++_size;
      return true;
    }

    // Update existing
    if (_keys[pos] == key) {
      _values [pos] = value;
      return true;
    }
  }
  return false;
}

// erase key if exists
inline void erase(uint64_t key) noexcept {
  const uint64_t h = hash_(key);
  const int8_t tag = static_cast<int8_t>(h & 0x7F);
  const size_t idx = (h >> 7) & (SLOTS - 1);
  for (size_t i = 0; i < SLOTS; ++i) {
    const size_t pos = (idx + i) & (SLOTS - 1);
    const int8_t c = _ctrl[pos];

    if (c == Control::Empty) return;
    if (c == tag && _keys[pos] == key) {
      set_ctrl_(pos, Control::Deleted);
      _values[pos] = nullptr;
      if (_size) --_size;
      // ++_deleted;
      return;
    }
  }
}
inline size_t size() const noexcept { return _size; }
static constexpr size_t capacity() noexcept { return SLOTS; }

inline void clear() noexcept {
   for (size_t i = 0; i < SLOTS; ++i) {
    _ctrl[i]   = Control::Empty;
    _keys[i]   = 0;
    _values[i] = nullptr;
  }
  mirror_tail_();
  _size = 0;
  // _deleted = 0;
}


  // copied from https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
  static inline uint64_t hash_(uint64_t k) noexcept {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
  }

private:
  // copy first SIM_SIZE control bytes to tail (indices [SLOTS .. SLOTS+SIMD_SIZE-1])
  inline void mirror_tail_() noexcept {
    std::memcpy (&_ctrl[SLOTS], &_ctrl[0], SIMD_SIZE);
  }

  // set control byte and maintain tail mirror when touching head

  inline void set_ctrl_(size_t pos, int8_t v) noexcept {
    _ctrl[pos] = v;
    if (pos < SIMD_SIZE) _ctrl[SLOTS + pos] = v;
  }

  // control bytes with tail padding to allow safe 16-byte Loads near the end
  alignas(16) std::array<int8_t, SLOTS + SIMD_SIZE> _ctrl;
  std::array<uint64_t, SLOTS> _keys;
  std::array<Value*, SLOTS> _values;
  size_t _size;
  // size_t _deleted;
  // TODO consider using to rebuild hash table
  // too many deleted slots impact searching when key doesn't exist
  // which is not an issue in our use case

};

}

// --- END FILE: include/hw/utility/cce/HashTable.hpp ---
