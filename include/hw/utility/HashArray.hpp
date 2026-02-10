#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <cstring> // for memcpy

#include <immintrin.h>

namespace hw::utility {

// Thread Safety Policy
enum class ThreadSafetyPolicy { Single, Multi };

// Insert Result
enum class InsertResult { Success, DuplicateKey, TableFull };

/**
 * @brief Base template for HashArray implementations.
 *    Specialized below for ThreadSafetyPolicy.
 */
template <typename KeyType, typename ValueType, size_t MAX_KEYS, ThreadSafetyPolicy Policy>
class HashArrayBase;

/**
 * @brief Single-Threaded Implementation (HashArrayST)
 *    - No atomics (uses raw int8_t for control).
 *    - No spin-waits.
 *    - Uses Mirrored Tail for simplified SIMD reads (Control array size + 16).
 *    - Optimized for single thread.
 */
template <typename KeyType, typename ValueType, size_t MAX_KEYS>
class HashArrayBase<KeyType, ValueType, MAX_KEYS, ThreadSafetyPolicy::Single> {
  static_assert((MAX_KEYS & (MAX_KEYS - 1)) == 0, "MAX_KEYS must be a power of 2");
  static_assert(MAX_KEYS >= 16, "MAX_KEYS must be at least 16 for SIMD probing");

public:
  HashArrayBase() {
  _ctrl.fill(Control::Empty);
  }

  InsertResult insert(const KeyType &key, ValueType *value) noexcept {
  const uint64_t h = key.hash();
  const int8_t tag = static_cast<int8_t>(h & 0x7F);
  const size_t start_idx = (h >> 7) & (MAX_KEYS - 1);

  for (size_t i = 0; i < MAX_KEYS; ++i) {
    const size_t idx = (start_idx + i) & (MAX_KEYS - 1);
    int8_t c = _ctrl[idx];

    if (c == tag) {
    if (_keys[idx] == key) return InsertResult::DuplicateKey;
    } else if (c == Control::Empty) {
    _ctrl[idx] = tag;
    if (idx < 16) _ctrl[MAX_KEYS + idx] = tag; // Update Mirror
    _keys[idx] = key;
    _data[idx] = value;
    return InsertResult::Success;
    }
  }
  return InsertResult::TableFull;
  }

  ValueType *find(const KeyType &key) const noexcept {
  const uint64_t h = key.hash();
  const int8_t tag = static_cast<int8_t>(h & 0x7F);
  const size_t start_idx = (h >> 7) & (MAX_KEYS - 1);

  for (size_t i = 0; i < MAX_KEYS; i += 16) {
    const size_t group_start = (start_idx + i) & (MAX_KEYS - 1);

    // Always safe to load 16 bytes due to mirrored tail
    __m128i ctrl_group = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_ctrl[group_start]));
    __m128i match_mask = _mm_cmpeq_epi8(ctrl_group, _mm_set1_epi8(tag));
    __m128i empty_mask = _mm_cmpeq_epi8(ctrl_group, _mm_set1_epi8(Control::Empty));
    
    uint32_t matches = _mm_movemask_epi8(match_mask);
    uint32_t empties = _mm_movemask_epi8(empty_mask);
    
    while (matches) {
    int bit = __builtin_ctz(matches);
    size_t idx = (group_start + bit) & (MAX_KEYS - 1); // Mask for key/value access
    if (_keys[idx] == key) return _data[idx];
    matches &= ~(1 << bit);
    }
    
    if (empties) return nullptr; // Found empty, stop probing
  }
  return nullptr;
  }

  template <typename Fn>
  void for_each(Fn &&f) const noexcept {
  for (size_t i = 0; i < MAX_KEYS; ++i) {
    int8_t c = _ctrl[i];
    if (c != Control::Empty) {
    f(_keys[i], _data[i]);
    }
  }
  }

private:
  struct Control {
  static constexpr int8_t Empty = static_cast<int8_t>(0xFF);
  };

  alignas(16) std::array<int8_t, MAX_KEYS + 16> _ctrl; // +16 for Mirror
  std::array<KeyType, MAX_KEYS> _keys;
  std::array<ValueType*, MAX_KEYS> _data;
};

/**
 * @brief Thread-Safe Implementation (HashArrayMT)
 *    - Uses atomics.
 *    - Uses spin-waits ( _mm_pause ) for contention.
 *    - Uses Scalar fallback at tail (No atomic mirroring).
 */
template <typename KeyType, typename ValueType, size_t MAX_KEYS>
class HashArrayBase<KeyType, ValueType, MAX_KEYS, ThreadSafetyPolicy::Multi> {
  static_assert((MAX_KEYS & (MAX_KEYS - 1)) == 0, "MAX_KEYS must be a power of 2");
  static_assert(MAX_KEYS >= 16, "MAX_KEYS must be at least 16 for SIMD probing");

public:
  HashArrayBase() {
  for (auto& c : _ctrl) c.store(Control::Empty, std::memory_order_relaxed);
  }

  InsertResult insert(const KeyType &key, ValueType *value) noexcept {
  const uint64_t h = key.hash();
  const int8_t tag = static_cast<int8_t>(h & 0x7F);
  const size_t start_idx = (h >> 7) & (MAX_KEYS - 1);

  for (size_t i = 0; i < MAX_KEYS; ++i) {
    const size_t idx = (start_idx + i) & (MAX_KEYS - 1);
    int8_t c = _ctrl[idx].load(std::memory_order_acquire); // Acquire to see 'tag'

    if (c == tag) {
    if (_keys[idx] == key) return InsertResult::DuplicateKey;
    } 
    
    if (c == Control::Empty) {
    if (_ctrl[idx].compare_exchange_strong(c, Control::Busy, 
                         std::memory_order_acq_rel, 
                         std::memory_order_acquire)) {
      _keys[idx] = key;
      _data[idx] = value;
      _ctrl[idx].store(tag, std::memory_order_release); // Release so find() sees data
      return InsertResult::Success;
    }
    i--; continue; // Retry slot
    }

    if (c == Control::Busy) {
    _mm_pause();
    i--; continue; // Retry slot
    }
  }
  return InsertResult::TableFull;
  }

  ValueType *find(const KeyType &key) const noexcept {
  const uint64_t h = key.hash();
  const int8_t tag = static_cast<int8_t>(h & 0x7F);
  const size_t start_idx = (h >> 7) & (MAX_KEYS - 1);

  for (size_t i = 0; i < MAX_KEYS; i += 16) {
    const size_t group_start = (start_idx + i) & (MAX_KEYS - 1);

    if (group_start + 16 <= MAX_KEYS) {
    // Relaxed load for SIMD scan ("dirty" read)
    __m128i ctrl_group = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_ctrl[group_start]));
    __m128i match_mask = _mm_cmpeq_epi8(ctrl_group, _mm_set1_epi8(tag));
    __m128i empty_mask = _mm_cmpeq_epi8(ctrl_group, _mm_set1_epi8(Control::Empty));
    
    uint32_t matches = _mm_movemask_epi8(match_mask);
    uint32_t empties = _mm_movemask_epi8(empty_mask);
    
    while (matches) {
      int bit = __builtin_ctz(matches);
      size_t idx = group_start + bit;
      
      // CONFIRM with Acquire load
      if (_ctrl[idx].load(std::memory_order_acquire) == tag) {
      if (_keys[idx] == key) return _data[idx]; // Data visible due to acquire
      }
      matches &= ~(1 << bit);
    }
    
    if (empties) {
      // Check first empty candidate with Acquire to confirm it's truly empty
      int bit = __builtin_ctz(empties);
      size_t idx = group_start + bit;
      if (_ctrl[idx].load(std::memory_order_acquire) == Control::Empty) {
      return nullptr;
      }
      // If race caused it to not be empty, continue probing
    }
    } else {
    // Scalar fallback for wrap-around
    for (size_t k = 0; k < 16; ++k) {
      size_t idx = (group_start + k) & (MAX_KEYS - 1);
      int8_t c = _ctrl[idx].load(std::memory_order_acquire);
      if (c == tag && _keys[idx] == key) return _data[idx];
      if (c == Control::Empty) return nullptr;
    }
    }
  }
  return nullptr;
  }

  template <typename Fn>
  void for_each(Fn &&f) const noexcept {
  for (size_t i = 0; i < MAX_KEYS; ++i) {
    int8_t c = _ctrl[i].load(std::memory_order_acquire);
    if (c != Control::Empty && c != Control::Busy) {
    f(_keys[i], _data[i]);
    }
  }
  }

private:
  struct Control {
  static constexpr int8_t Empty = static_cast<int8_t>(0xFF);
  static constexpr int8_t Busy  = static_cast<int8_t>(0xFE);
  };

  alignas(16) std::array<std::atomic<int8_t>, MAX_KEYS> _ctrl;
  std::array<KeyType, MAX_KEYS> _keys;
  std::array<ValueType*, MAX_KEYS> _data;
};

/**
 * @brief HashArray Main Class
 *    Selects implementation based on Policy.
 */
template <typename KeyType, typename ValueType, size_t MAX_KEYS, ThreadSafetyPolicy Policy = ThreadSafetyPolicy::Multi>
class HashArray : public HashArrayBase<KeyType, ValueType, MAX_KEYS, Policy> {
public:
  using Base = HashArrayBase<KeyType, ValueType, MAX_KEYS, Policy>;
  using Base::Base;
};

/**
 * @brief Generic Key Template for HashArray
 *    Expects sizeof(Type) to match SIZE (for casting).
 *    If SIZE >= 16, assumes it might benefit from SIMD/memcmp optimizations.
 */
template <size_t SIZE>
class Key {
public:
  // Helper to view raw bytes as Type
  template <typename Type>
  Type *data() noexcept { return reinterpret_cast<Type*>(_data); }
  
  template <typename Type>
  const Type *data() const noexcept { return reinterpret_cast<const Type*>(_data); }

  // Helper to view raw bytes directly
  std::byte* raw() noexcept { return _data; }
  const std::byte* raw() const noexcept { return _data; }

  uint64_t hash() const noexcept {
  // FNV-1a hash
  uint64_t hash = 14695981039346656037ULL;
  for (size_t i = 0; i < SIZE; ++i) {
    hash ^= static_cast<uint8_t>(_data[i]);
    hash *= 1099511628211ULL;
  }
  return hash;
  }

  bool operator==(const Key &other) const noexcept {
  return std::memcmp(_data, other._data, SIZE) == 0;
  }

private:
  alignas(8) std::byte _data[SIZE];
};

} // namespace hw::utility
