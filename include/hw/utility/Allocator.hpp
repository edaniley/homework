// --- START FILE: include/hw/utility/Allocator.hpp ---
#pragma once

#include <cstdlib> // For posix_memalign, free
#include <new>     // For placement new
#include <limits>
#include <vector>
#include <cstring> // For std::memcpy (if needed)
#include <utility> // For std::forward
#include <algorithm> // For std::max

namespace hw::utility {

/**
 * AllocatorTrivial: A high-performance pool allocator for a specific Type.
 * - Uses an embedded free-list to eliminate runtime heap allocations for Type objects.
 * - Guarantees ALIGNMENT for Type.
 * - Provides standard allocation/deallocation of raw memory, and separate construct/destroy.
 */
template <typename Type>
class AllocatorTrivial {
  // A node in our free-list, stored directly in the idle memory blocks
  struct Node {
    Node* next;
  };

  // Ensure the block size is large enough to hold either Type or Node, and is aligned.
  // This ensures that when a block is free, it can store a Node*, and when allocated, it fits Type.
  static constexpr size_t BLOCK_SIZE = (std::max(sizeof(Type), sizeof(Node)) + alignof(Type) - 1) & ~(alignof(Type) - 1);
  static constexpr size_t ALIGNMENT = alignof(Type);

public:
  explicit AllocatorTrivial(size_t count) : _count(count) {
    if (_count == 0) {
        _data = nullptr; // Handle zero-sized allocation
        return;
    }
    // 1. Aligned allocation of the main pool
    // posix_memalign ensures we respect Type's alignment requirements
    if (int err = posix_memalign(reinterpret_cast<void**>(&_data), ALIGNMENT, _count * BLOCK_SIZE)) {
      throw std::bad_alloc();
    }

    std::byte* ptr = reinterpret_cast<std::byte*>(_data);
    for (size_t i = 0; i < _count; ++i) {
      push(ptr); // Push raw pointers to the free list
      ptr += BLOCK_SIZE;
    }
  }

  ~AllocatorTrivial() {
    // Free any memory obtained via fallback _postalloc
    for (void* ptr : _postalloc) {
      ::free(ptr);
    }
    // Free the main pre-allocated pool
    ::free(_data);
  }

  AllocatorTrivial(const AllocatorTrivial&) = delete;
  AllocatorTrivial& operator=(const AllocatorTrivial&) = delete;
  // No move constructor/assignment needed for this use case. Delete for simplicity.
  AllocatorTrivial(AllocatorTrivial&&) = delete;
  AllocatorTrivial& operator=(AllocatorTrivial&&) = delete;

  // Allocate raw memory for one object of Type.
  // Returns a pointer to uninitialized memory.
  inline Type* allocate() {
    void* ptr = nullptr;

    if (_free) [[likely]] {
      ptr = _free; // Take from free list
      _free = _free->next;
    } else {
      // Fallback: This is the "jitter" path - indicates initial pool might be too small.
      // Consider logging this in a real system.
      if (int err = posix_memalign(&ptr, ALIGNMENT, BLOCK_SIZE)) {
        throw std::bad_alloc();
      }
      _postalloc.push_back(ptr); // Keep track of fallback allocations for freeing
    }
    return reinterpret_cast<Type*>(ptr);
  }

  // Deallocate raw memory.
  inline void free(Type* ptr) {
    if (!ptr) return;
    // Note: The destructor for Type is NOT called here. Caller is responsible for destroy.
    push(reinterpret_cast<std::byte*>(ptr));
  }

  // Construct an object of Type at the given pre-allocated memory location.
  template <typename ... Args>
  inline void construct(Type* ptr, Args&&... args) {
    new (ptr) Type(std::forward<Args>(args)...);
  }

  // Destroy an object of Type at the given memory location.
  inline void destroy(Type* ptr) {
    if (ptr) {
      ptr->~Type();
    }
  }

  // Get a pointer to the Type object at a specific index within the main pre-allocated pool.
  // This method is useful for direct indexed access, assuming the index is valid.
  Type* get(size_t idx) noexcept {
      if (idx < _count) {
          // Calculate address: _data + idx * BLOCK_SIZE
          return reinterpret_cast<Type*>(static_cast<std::byte*>(_data) + idx * BLOCK_SIZE);
      }
      return nullptr; // Index out of bounds of the initial pool
  }

  const Type* get(size_t idx) const noexcept {
      if (idx < _count) {
          return reinterpret_cast<const Type*>(static_cast<const std::byte*>(_data) + idx * BLOCK_SIZE);
      }
      return nullptr;
  }

private:
  // Push a raw memory block to the free list.
  inline void push(std::byte* ptr) {
    Node* node = reinterpret_cast<Node*>(ptr);
    node->next = _free;
    _free = node;
  }

  void* _data = nullptr; // Pointer to the start of the main pre-allocated memory pool
  Node* _free = nullptr; // Head of the free list
  size_t _count = 0;   // Number of blocks in the initial pool

  // Stores pointers to memory blocks allocated during fallback (when free list is empty)
  std::vector<void*> _postalloc;
};

} // namespace hw::utility
// --- END FILE: include/hw/utility/Allocator.hpp ---
