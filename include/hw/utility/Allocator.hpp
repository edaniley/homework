// --- START FILE: include/hw/utility/Allocator.hpp ---
#pragma once

#include <cstdlib>
#include <new>
#include <limits>
#include <vector>
#include <cstring>
#include <utility>

namespace hw::utility {

/**
 * AllocatorTrivial: A high-performance pool allocator for a specific Type.
 * - Uses an embedded free-list to eliminate container overhead.
 * - Guarantees ALIGNMENT for Type.
 */
template <typename Type>
class AllocatorTrivial {
  // A node in our free-list, stored directly in the idle memory blocks
  struct Node {
    Node* next;
  };

  // Ensure the type is large enough to hold our next pointer
  static constexpr size_t BLOCK_SIZE = std::max(sizeof(Type), sizeof(Node));
  static constexpr size_t ALIGNMENT = alignof(Type);

public:
  explicit AllocatorTrivial(size_t count) : _count(count) {
    // 1. Aligned allocation of the main pool
    // posix_memalign or aligned_alloc ensures we respect Type's alignment requirements
    if (int err = posix_memalign(reinterpret_cast<void**>(&_data), ALIGNMENT, _count * BLOCK_SIZE)) {
      throw std::bad_alloc();
    }


    std::byte* ptr = reinterpret_cast<std::byte*>(_data);
    for (size_t i = 0; i < _count; ++i) {
      push(reinterpret_cast<Type*>(ptr));
      ptr += BLOCK_SIZE;
    }
  }

  ~AllocatorTrivial() {
    for (void* ptr : _postalloc) {
      ::free(ptr);
    }
    ::free(_data);
  }

  AllocatorTrivial(const AllocatorTrivial&) = delete;
  AllocatorTrivial& operator=(const AllocatorTrivial&) = delete;

  template <typename ... Args>
  inline Type* allocate(Args&&... args) {
    void* ptr = nullptr;

    if (_free) [[likely]] {
      ptr = _free;
      _free = _free->next;
    } else {
      // Fallback: This is the "jitter" path. Consider logging/tracing this.
      if (int err = posix_memalign(&ptr, ALIGNMENT, BLOCK_SIZE)) {
        throw std::bad_alloc();
      }
      _postalloc.push_back(ptr);
    }

    return new (ptr) Type(std::forward<Args>(args)...);
  }

  inline void free(Type* ptr) {
    if (!ptr) return;
    ptr->~Type();
    push(ptr);
  }

private:
  inline void push(Type* ptr) {
    Node* node = reinterpret_cast<Node*>(ptr);
    node->next = _free;
    _free = node;
  }

  void* _data = nullptr;
  Node* _free = nullptr;
  size_t _count = 0;

  // Use vector for post-alloc to be slightly leaner than deque
  std::vector<void*> _postalloc;
};

} // namespace hw::utility
// --- END FILE: include/hw/utility/Allocator.hpp ---
