#pragma once

#if !defined(__SSE2__)
  #error "This header requires SSE2 instructions to be enabled. Please use -msse2 or appropriate compiler flags."
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib> // aligned_alloc, free
#include <cstring>
#include <memory>
#include <new>     // std::launder
#include <stdexcept>
#include <type_traits>
#include <algorithm>
#include <bit>

#ifdef DEBUG
	#include <iostream>
	#include <bitset>
#endif

#include <immintrin.h>
#include <hw/utility/HashArray.hpp>

namespace hw::utility::swisstable {

// definitions available via HashArray.hpp
// static constexpr size_t SIMD_SIZE = 16;
// enum class InsertResult { Success, DuplicateKey, TableFull };

template <typename KeyType, typename ValueType, bool THREAD_SAFE>
class HashVarrayBase;

//
//  Single-Threaded Implementation
//
template <typename KeyType, typename ValueType>
class HashVarrayBase<KeyType, ValueType, false> {
  static_assert(std::is_trivial_v<KeyType> && std::is_trivially_destructible_v<KeyType>, "KeyType must be a trivial type (POD) and not require a destructor.");
public:
  size_t capacity() const { return _capacity; }
  HashVarrayBase(size_t max_keys) :
    _capacity([max_keys]() {
      size_t new_max_keys = std::bit_ceil(max_keys);
      // Ensure it's at least SIMD_SIZE
      if (new_max_keys < SIMD_SIZE) {
          new_max_keys = SIMD_SIZE;
      }
      return new_max_keys;
    }()),
    _mask(_capacity - 1)
  {

    allocate_memory();
    // Initialize control bytes to Empty
    std::memset(_ctrl, static_cast<int>(Control::Empty), _capacity + SIMD_SIZE);
    
    // Default construct keys? 
    // std::array default constructs elements. We should do the same for consistency.
    if constexpr (!std::is_trivially_default_constructible_v<KeyType>) {
        for (size_t i = 0; i < _capacity; ++i) {
            new (&_keys[i]) KeyType();
        }
    }
  }

  ~HashVarrayBase() {
    if constexpr (!std::is_trivially_destructible_v<KeyType>) {
        for (size_t i = 0; i < _capacity; ++i) {
            _keys[i].~KeyType();
        }
    }
    std::free(_memory_block);
  }

  // Delete copy/move for simplicity (HashArray didn't specify, but managing raw ptrs requires care)
  HashVarrayBase(const HashVarrayBase&) = delete;
  HashVarrayBase& operator=(const HashVarrayBase&) = delete;
  HashVarrayBase(HashVarrayBase&&) = delete; 
  HashVarrayBase& operator=(HashVarrayBase&&) = delete;

  InsertResult insert(const KeyType & key, ValueType * value) noexcept {
    const uint64_t hashval = key.hash();
    const int8_t tag = static_cast<int8_t>(hashval & 0x7F);
    const size_t start_idx = (hashval >> 7) & _mask;

    for (size_t i = 0; i < _capacity; ++i) {
      const size_t idx = (start_idx + i) & _mask;
      int8_t ctrl = _ctrl[idx];

      if (ctrl == tag) {
        if (_keys[idx] == key) {
          return InsertResult::DuplicateKey;
        }
      }
      else if (ctrl == Control::Empty) {
        _ctrl[idx] = tag;
        if (idx < SIMD_SIZE) {
          _ctrl[_capacity + idx] = tag; // update mirrored tail
        }
        _keys[idx] = key;
        _data[idx] = value;
        return InsertResult::Success;
      }
    }
    return InsertResult::TableFull;
  }

  ValueType * find(const KeyType & key) const noexcept {
    const uint64_t hashval = key.hash();
    const int8_t tag = static_cast<int8_t>(hashval & 0x7F);
    const size_t start_idx = (hashval >> 7) & _mask;

    for (size_t i = 0; i < _capacity; i += SIMD_SIZE) {
      const size_t group_idx = (start_idx + i) & _mask;
      
      __m128i group = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_ctrl[group_idx]));
      const __m128i matchedBytes = _mm_cmpeq_epi8(group, _mm_set1_epi8(tag));
      uint32_t matchedBits = _mm_movemask_epi8(matchedBytes);

      while (matchedBits) {
        int bit = __builtin_ctz(matchedBits); 
        size_t idx = (group_idx + bit) & _mask;
        if (_keys[idx] == key) [[likely]] {
          return _data[idx];
        }
        matchedBits &= ~(1 << bit);
      }
      
      const uint32_t emptyMask = static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(group, _mm_set1_epi8(Control::Empty))));
      if (emptyMask) [[likely]] return nullptr;
    }
    return nullptr;
  }

  template <typename Fn>
  void for_each(Fn &&f) const noexcept {
    for (size_t i = 0; i < _capacity; ++i) {
      int8_t ctrl = _ctrl[i];
      if (ctrl != Control::Empty) {
        f(_keys[i], _data[i]);
      }
    }
  }

private:
  void allocate_memory() {
    // Layout:
    // 1. Control bytes: _capacity + SIMD_SIZE (aligned 16)
    // 2. Keys: _capacity (aligned alignof(KeyType))
    // 3. Data pointers: _capacity (aligned alignof(ValueType*))

    size_t ctrl_size = (_capacity + SIMD_SIZE) * sizeof(int8_t);
    size_t ctrl_aligned_size = (ctrl_size + alignof(KeyType) - 1) & ~(alignof(KeyType) - 1);
    
    size_t keys_size = _capacity * sizeof(KeyType);
    size_t keys_aligned_size = (keys_size + alignof(ValueType*) - 1) & ~(alignof(ValueType*) - 1);
    
    size_t data_size = _capacity * sizeof(ValueType*);
    
    // Offset for keys
    size_t keys_offset = ctrl_aligned_size;
    // Offset for data
    size_t data_offset = keys_offset + keys_aligned_size;
    
    size_t total_size = data_offset + data_size;
    size_t alignment = std::max({size_t(16), alignof(KeyType), alignof(ValueType*)});

    _memory_block = std::aligned_alloc(alignment, (total_size + alignment - 1) & ~(alignment - 1));
    if (!_memory_block) throw std::bad_alloc();

    char* base = static_cast<char*>(_memory_block);
    _ctrl = reinterpret_cast<int8_t*>(base);
    _keys = reinterpret_cast<KeyType*>(base + keys_offset);
    _data = reinterpret_cast<ValueType**>(base + data_offset);
  }

  struct Control {
    static constexpr int8_t Empty = static_cast<int8_t>(0xFF);
  };

  const size_t _capacity;
  const size_t _mask;
  
  void* _memory_block = nullptr;
  int8_t* _ctrl = nullptr;
  KeyType* _keys = nullptr;
  ValueType** _data = nullptr;
};

//
//  Thread-Safe Implementation
//
template <typename KeyType, typename ValueType>
class HashVarrayBase<KeyType, ValueType, true> {
  static_assert(std::is_trivial_v<KeyType> && std::is_trivially_destructible_v<KeyType>, "KeyType must be a trivial type (POD) and not require a destructor.");
public:
  size_t capacity() const { return _capacity; }
  HashVarrayBase(size_t max_keys_param) :
    _capacity([&]() {
      size_t new_max_keys = std::bit_ceil(max_keys_param);
      // Ensure it's at least SIMD_SIZE
      if (new_max_keys < SIMD_SIZE) {
          new_max_keys = SIMD_SIZE;
      }
      return new_max_keys;
    }()),
    _mask(_capacity - 1)
  {
    allocate_memory();
    
    // Initialize atomic controls
    for (size_t i = 0; i < _capacity; ++i) {
      _ctrl[i].store(Control::Empty, std::memory_order_relaxed);
    }
    // No mirror tail in MT version of HashArray, but let's check HashArray.hpp
    // HashArrayBase<..., true> in HashArray.hpp declares "_ctrl" as "std::array<std::atomic<int8_t>, MAX_KEYS>".
    // It DOES NOT have +SIMD_SIZE mirror in MT implementation logic in HashArray.hpp.
    // The find() logic handles wrap-around explicitly.
    
    if constexpr (!std::is_trivially_default_constructible_v<KeyType>) {
        for (size_t i = 0; i < _capacity; ++i) {
            new (&_keys[i]) KeyType();
        }
    }
  }

  ~HashVarrayBase() {
    if constexpr (!std::is_trivially_destructible_v<KeyType>) {
        for (size_t i = 0; i < _capacity; ++i) {
            _keys[i].~KeyType();
        }
    }
    // We need to destroy atomics? std::atomic is trivially destructible usually.
    std::free(_memory_block);
  }
  
  // Delete copy/move
  HashVarrayBase(const HashVarrayBase&) = delete;
  HashVarrayBase& operator=(const HashVarrayBase&) = delete;
  HashVarrayBase(HashVarrayBase&&) = delete; 
  HashVarrayBase& operator=(HashVarrayBase&&) = delete;

  InsertResult insert(const KeyType & key, ValueType * value) noexcept {
    const uint64_t hashval = key.hash();
    const int8_t tag = static_cast<int8_t>(hashval & 0x7F);
    const size_t start_idx = (hashval >> 7) & _mask;

    for (size_t i = 0; i < _capacity; ++i) {
      const size_t idx = (start_idx + i) & _mask;
      int8_t ctrl = _ctrl[idx].load(std::memory_order_acquire);

      if (ctrl == tag) {
        if (_keys[idx] == key) return InsertResult::DuplicateKey;
      }

      if (ctrl == Control::Empty) {
        if (_ctrl[idx].compare_exchange_strong(ctrl, Control::Busy,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) [[likely]] {
          _keys[idx] = key;
          _data[idx] = value;
          _ctrl[idx].store(tag, std::memory_order_release);
          return InsertResult::Success;
        }
        i--;
        continue; // retry
      }

      if (ctrl == Control::Busy) [[unlikely]] {
        _mm_pause();
        i--;
        continue; // retry
      }
    }
    return InsertResult::TableFull;
  }

  ValueType * find(const KeyType & key) const noexcept {
    const uint64_t hashval = key.hash();
    const int8_t tag = static_cast<int8_t>(hashval & 0x7F);
    const size_t start_idx = (hashval >> 7) & _mask;

    for (size_t i = 0; i < _capacity; i += SIMD_SIZE) {
      const size_t group_idx = (start_idx + i) & _mask;

      if (group_idx + SIMD_SIZE <= _capacity) {
        const __m128i group = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_ctrl[group_idx]));
        const __m128i matchedBytes = _mm_cmpeq_epi8(group, _mm_set1_epi8(tag));

        uint32_t matchedBits = _mm_movemask_epi8(matchedBytes);
        while (matchedBits) {
          int bit = __builtin_ctz(matchedBits);
          size_t idx = group_idx + bit;

          // confirm match
          if (_ctrl[idx].load(std::memory_order_acquire) == tag) {
            if (_keys[idx] == key) [[likely]] {
              return _data[idx];
            }
          }
          matchedBits &= ~(1 << bit); 
        }

        const __m128i emptyMask = _mm_cmpeq_epi8(group, _mm_set1_epi8(Control::Empty));
        uint32_t emptyBits = _mm_movemask_epi8(emptyMask);
        if (emptyBits) {
          // Check all empty candidates with Acquire to confirm they are truly empty
          while (emptyBits) {
            int bit = __builtin_ctz(emptyBits);
            size_t idx = group_idx + bit;
            if (_ctrl[idx].load(std::memory_order_acquire) == Control::Empty) {
              return nullptr;
            }
            // If race caused it to not be empty, try next empty bit
            emptyBits &= ~(1 << bit);
          }
        }
      }
      else { 
        // we are close to the end
        // we cannot maintain mirror group atomically
        // check byte by byte for wrap-around
        for (size_t k = 0; k < SIMD_SIZE; ++k) {
          size_t idx = (group_idx + k) & _mask;
          int8_t ctrl = _ctrl[idx].load(std::memory_order_acquire);
          if (ctrl == tag && _keys[idx] == key) return _data[idx];
          if (ctrl == Control::Empty) return nullptr;
        }
       }
    }
    return nullptr;
  }

  template <typename Fn>
  void for_each(Fn &&f) const noexcept {
    for (size_t i = 0; i < _capacity; ++i) {
      int8_t ctrl = _ctrl[i].load(std::memory_order_acquire);
      if (ctrl != Control::Empty && ctrl != Control::Busy) {
        f(_keys[i], _data[i]);
      }
    }
  }

private:
  void allocate_memory() {
    // Layout for MT:
    // 1. Control bytes (Atomic): _capacity (aligned 16)
    // 2. Keys: _capacity
    // 3. Data: _capacity
    
    // Check atomic assumptions
    static_assert(sizeof(std::atomic<int8_t>) == sizeof(int8_t), "Atomic int8_t size mismatch");
    static_assert(alignof(std::atomic<int8_t>) == alignof(int8_t), "Atomic int8_t alignment mismatch");

    size_t ctrl_size = _capacity * sizeof(std::atomic<int8_t>);
    size_t ctrl_aligned_size = (ctrl_size + alignof(KeyType) - 1) & ~(alignof(KeyType) - 1);
    
    size_t keys_size = _capacity * sizeof(KeyType);
    size_t keys_aligned_size = (keys_size + alignof(ValueType*) - 1) & ~(alignof(ValueType*) - 1);
    
    size_t data_size = _capacity * sizeof(ValueType*);
    
    size_t keys_offset = ctrl_aligned_size;
    size_t data_offset = keys_offset + keys_aligned_size;
    
    size_t total_size = data_offset + data_size;
    size_t alignment = std::max({size_t(16), alignof(KeyType), alignof(ValueType*)});

    _memory_block = std::aligned_alloc(alignment, (total_size + alignment - 1) & ~(alignment - 1));
    if (!_memory_block) throw std::bad_alloc();

    char* base = static_cast<char*>(_memory_block);
    _ctrl = reinterpret_cast<std::atomic<int8_t>*>(base);
    _keys = reinterpret_cast<KeyType*>(base + keys_offset);
    _data = reinterpret_cast<ValueType**>(base + data_offset);
  }

  struct Control {
    static constexpr int8_t Empty = static_cast<int8_t>(0xFF);
    static constexpr int8_t Busy  = static_cast<int8_t>(0xFE);
  };

  const size_t _capacity;
  const size_t _mask;
  
  void* _memory_block = nullptr;
  std::atomic<int8_t>* _ctrl = nullptr;
  KeyType* _keys = nullptr;
  ValueType** _data = nullptr;
};

template <typename KeyType, typename ValueType, bool THREAD_SAFE = true>
class HashVarray : public HashVarrayBase<KeyType, ValueType, THREAD_SAFE> {
public:
  using Base = HashVarrayBase<KeyType, ValueType, THREAD_SAFE>;
  using Base::Base;
  using Base::capacity;

};

} // namespace hw::utility::swisstable
