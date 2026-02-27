#pragma once

#include <array>
#include <vector>
#include <stack>
#include <chrono>
#include <numeric>      // For std::iota
#include <algorithm>    // For std::fill, std::max
#include <cstddef>      // For std::byte, std::size_t
#include <functional>   // For std::hash
#include <new>          // For placement new
#include <stdexcept>    // For std::bad_alloc
#include <utility>      // For std::forward, std::pair

#include <hw/utility/Spinlock.hpp>
#include <hw/utility/Allocator.hpp> // Using your AllocatorTrivial
#include <hw/utility/HashTableTrivial.hpp> // Using the new custom hash table

namespace hw::utility::cce {

template <typename Type, size_t KeySize>
class Table {
public:
  // Enforce POD type. std::is_trivial covers trivial constructor/destructor/copy/move.
  // std::is_standard_layout ensures layout compatible with C, important for reinterpret_cast.
  static_assert(std::is_trivial_v<Type> && std::is_standard_layout_v<Type>, "Type must be a POD (Plain Old Data) type.");
  
  // Use separate cache line for each Type. Ensure AllocSize is a multiple of 64.
  static constexpr size_t AllocSize = (sizeof(Type) + 63) & ~63; 
  using KeyType = std::array<std::byte, KeySize>;
  // AllocType is no longer directly used for _pool storage, as AllocatorTrivial manages Type directly.

  // Custom hash and equality for KeyType (std::array<std::byte, N>)
  // These are defined here to pass to HashTableTrivial and for internal helper methods.
  struct OpaqueKeyHash {
      std::size_t operator()(const KeyType& k) const noexcept {
          std::size_t seed = 0; 
          for (std::size_t i = 0; i < KeySize; ++i) {
              seed ^= static_cast<std::size_t>(k[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
          }
          return seed;
      }
  };

  struct OpaqueKeyEqual {
      bool operator()(const KeyType& a, const KeyType& b) const noexcept {
          return a == b;
      }
  };

  // Our internal hash table will map KeyType to an int (index into _pool_allocator's memory).
  using InternalHashTable = hw::utility::HashTableTrivial<KeyType, int, OpaqueKeyHash, OpaqueKeyEqual>;

  explicit Table(size_t maxSize) 
    : _max_size(maxSize),
      _current_size(0),
      _index_list(maxSize, 0), // Initialize all indices as free (0)
      _index_pool(), // Stack will be populated below
      _pool_allocator(maxSize), // Initialize AllocatorTrivial for maxSize Type objects
      _hash_table(maxSize) // Initialize HashTableTrivial with capacity for maxSize keys
  {
      // Populate _index_pool with indices 0 to maxSize-1
      std::vector<int> initial_indices(maxSize);
      std::iota(initial_indices.begin(), initial_indices.end(), 0);
      for (int i : initial_indices) {
          _index_pool.push(i);
      }
  }

  // No need for custom destructor for Table if all members manage their own resources
  // and Type is trivial.
  ~Table() = default;

  // main use case (critical path)
  // ASSUMPTION: Type has a default constructor and an init(SomeType) method.
  // ASSUMPTION: Type has a public method to retrieve its KeyType, e.g., t->getKey().
  template <typename SomeType>
  void ProcessEntry(const KeyType &key, const SomeType & arg) {
    Type * t = nullptr;
    int pool_idx = -1;
    
    std::lock_guard<hw::utility::Spinlock> lock(_lock);
    
    // Find existing entry in custom hash table
    int* existing_pool_idx_ptr = _hash_table.find(key);
    if (existing_pool_idx_ptr != nullptr) {
      pool_idx = *existing_pool_idx_ptr; // Key found, use existing pool index
    } else [[unlikely]] {
      // Key not found, allocate a new entry
      pool_idx = allocate_pool_index();
      if (pool_idx == -1) [[unlikely]] {
        throw std::bad_alloc(); // No free slots in the pool
      }
      // Allocate raw memory for a Type object from _pool_allocator
      t = _pool_allocator.allocate(); // This gets a raw Type* from the pool
      _pool_allocator.construct(t); // Default construct Type (trivial for POD)
      t->init(static_cast<int>(arg)); // Initialize members using the provided argument
      
      // Insert into custom hash table (KeyType -> pool_idx)
      _hash_table.insert(key, pool_idx);
      _current_size++;
    }

    // Access Type and implement business logic (outside the lock if possible)
    // This assumes Type itself has internal synchronization or is accessed in a way that doesn't require the Table lock.
    if (t == nullptr) { // If it was an existing entry, t was not set in the locked block
        t = _pool_allocator.get(pool_idx); // Get Type* from pool by index
    }
    // use Type internal spin lock to access Type and implement business logic
    // e.g., t->update(arg); or t->do_something_with_lock();
  }

  // second important use case - slow path - remove stale entries
  // this is the reason i cannot use open addressing hash array
  size_t Cleanup (size_t startIdx , size_t length) {
    std::vector<KeyType> keys_to_erase; // Collect keys to erase, to avoid holding lock too long
    auto now = std::chrono::system_clock::now();

    size_t cleanup_count = 0;
    size_t current_idx_in_list = startIdx;

    // First pass: identify expired entries (can be done without the global lock if Type::expired is thread-safe)
    for (size_t l = 0; l < length; ++l) {
        size_t actual_pool_idx = current_idx_in_list % _max_size;

        // Only consider active entries (index_list[idx] == 1)
        if (_index_list[actual_pool_idx] == 1) {
            Type * t = _pool_allocator.get(actual_pool_idx);
            // ASSUMPTION: Type::expired(now) is thread-safe or only accesses immutable parts.
            if (t && t->expired(now)) {
                // ASSUMPTION: Type has a public method to retrieve its KeyType.
                keys_to_erase.push_back(t->getKey()); 
                cleanup_count++;
            }
        }
        current_idx_in_list++;
    }

    // Second pass: acquire lock and perform actual erasure and freeing
    if (!keys_to_erase.empty()) {
        std::lock_guard<hw::utility::Spinlock> lock(_lock);
        for (const auto& key : keys_to_erase) {
            // Erase from custom hash table. HashTableTrivial::erase will destroy Type and deallocate its memory.
            if (_hash_table.erase(key)) {
                _current_size--; 
                // Now, free the index back to our index pool.
                // The pool_idx is implicitly retrieved during _hash_table.erase by the HashTableTrivial Node destruction/deallocation.
                // However, CustomTable needs to manage its _index_list and _index_pool.
                // We need to retrieve the index from _hash_table.find(key) BEFORE erase to free it.
                int* idx_ptr = _hash_table.find(key); // Re-find key to get its index before it's gone.
                if (idx_ptr) {
                    free_pool_index(*idx_ptr);
                }
            }
        }
    }
    
    return startIdx + length; // Return next starting index (conceptual)
  }

  // Simplified Public Interface (as requested)
  int find (const KeyType &key) const noexcept {
    std::lock_guard<hw::utility::Spinlock> lock(_lock);
    const int* found_idx_ptr = _hash_table.find(key);
    if (found_idx_ptr != nullptr) {
        return *found_idx_ptr;
    }
    return -1;
  }

  bool remove (const KeyType &key) noexcept {
      std::lock_guard<hw::utility::Spinlock> lock(_lock);
      int* idx_ptr = _hash_table.find(key); // Find index before erasing
      if (idx_ptr) {
          int pool_idx_to_free = *idx_ptr;
          if (_hash_table.erase(key)) {
              _current_size--;
              free_pool_index(pool_idx_to_free);
              return true;
          }
      }
      return false;
  }

  size_t size() const noexcept {
    std::lock_guard<hw::utility::Spinlock> lock(_lock);
    return _hash_table.size(); // Delegating to HashTableTrivial's size
  }

  bool empty() const noexcept {
    std::lock_guard<hw::utility::Spinlock> lock(_lock);
    return _hash_table.empty(); // Delegating to HashTableTrivial's empty
  }

  void clear() noexcept {
      std::lock_guard<hw::utility::Spinlock> lock(_lock);

      // Iterate through keys in the hash table to get associated pool indices
      // and then clear the hash table.
      // Since HashTableTrivial doesn't provide iterators, we need to iterate _pool_allocator's elements
      // that are marked as `1` in `_index_list` and then `remove` them from the `_hash_table`.
      // This is a bit convoluted. A more direct approach to clear `HashTableTrivial` would be ideal.

      // Option 1: Iterate through the _index_list and remove from _hash_table
      // This is the safest way to ensure all Type objects are properly destructed and deallocated
      // and _index_list/_index_pool are correctly reset.
      for (size_t i = 0; i < _max_size; ++i) {
          if (_index_list[i] == 1) {
              Type* t = _pool_allocator.get(i);
              if (t) {
                  // We need the key to erase from _hash_table. This is where Type::getKey() is used.
                  KeyType key_to_remove = t->getKey();
                  // HashTableTrivial::erase will destroy Type and deallocate its memory.
                  // It also handles removal from _hash_table and decrementing its size.
                  _hash_table.erase(key_to_remove); 
                  // Mark index as free. No need to push to _index_pool, will be reset below.
                  _index_list[i] = 0;
              }
          }
      }

      _hash_table.clear(); // Ensure HashTableTrivial is completely clear
      
      // Reset _index_list and _index_pool
      std::fill(_index_list.begin(), _index_list.end(), 0);
      while (!_index_pool.empty()) _index_pool.pop();
      std::vector<int> initial_indices(_max_size);
      std::iota(initial_indices.begin(), initial_indices.end(), 0);
      for (int i : initial_indices) {
          _index_pool.push(i);
      }
      _current_size = 0;
  }

  // Public getter for Type object at a given pool index
  // NOTE: This assumes the caller has obtained a valid index from `find` or `ProcessEntry`.
  Type* get(size_t idx) noexcept {
      if (idx < _max_size && _index_list[idx] == 1) {
          return _pool_allocator.get(idx); // Get Type* from AllocatorTrivial by index
      }
      return nullptr; // Invalid index or not in use
  }

  const Type* get(size_t idx) const noexcept {
      if (idx < _max_size && _index_list[idx] == 1) {
          return _pool_allocator.get(idx); // Get Type* from AllocatorTrivial by index
      }
      return nullptr; // Invalid index or not in use
  }

  // Wrapper to get hash table distribution statistics
  void distribution(typename InternalHashTable::KeyDistribution& dist) const noexcept {
      std::lock_guard<hw::utility::Spinlock> lock(_lock);
      _hash_table.distribution(dist);
  }

private:
  // Allocates an index from the _index_pool (used to track free slots in _pool_allocator)
  int allocate_pool_index() {
    if (!_index_pool.empty()) {
      int idx = _index_pool.top(); // Get top element
      _index_pool.pop();           // Remove it
      _index_list[idx] = 1;        // Mark as used
      return idx;
    }
    return -1; // No free indices
  }

  // Returns an index to the _index_pool
  void free_pool_index(size_t idx) {
    // Ensure idx is valid before freeing
    if (idx < _max_size && _index_list[idx] == 1) {
      _index_pool.push(idx);
      _index_list[idx] = 0; // Mark as free
    } else {
        // Handle error: attempting to free an invalid or already free index
        // For a critical system, this might be an assert or throw.
        // std::cerr << "WARNING: Attempted to free invalid or already free index: " << idx << "\n";
    }
  }

  mutable hw::utility::Spinlock _lock; // Spinlock for thread-safety
  const size_t _max_size; // Total capacity of the table and pool
  size_t _current_size; // Number of actively occupied slots in the hash table (for _hash_table.size() compatibility)
  std::vector<int> _index_list; // 0 for free, 1 for used. Access guarded by _lock.
  std::stack<int, std::vector<int>> _index_pool; // Stack of available pool indices. Access guarded by _lock.
  
  hw::utility::AllocatorTrivial<Type> _pool_allocator; // Allocator for Type objects
  InternalHashTable _hash_table; // The internal separate chaining hash table
};

} // namespace hw::utility::cce
