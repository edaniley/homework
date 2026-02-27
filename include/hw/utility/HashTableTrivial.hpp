#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <bit> // For std::bit_ceil (C++20)
#include <cmath> // For std::ceil

#include <hw/utility/Allocator.hpp>

namespace hw::utility {

// HashTableTrivial: A non-thread-safe, fixed-size hash table using separate chaining.
// It uses AllocatorTrivial for its internal node allocations to prevent runtime heap access.
template <typename KeyType, typename PayloadType, typename Hash = std::hash<KeyType>, typename KeyEqual = std::equal_to<KeyType>>
class HashTableTrivial {
public:
  // Internal struct for hash table distribution statistics
  struct KeyDistribution {
    size_t bucketCnt = 0;
    size_t keyCnt = 0;
    size_t bucketUsedCnt = 0; // count of buckets with at least one key
    size_t collisionCnt = 0; // count of buckets with 2 or more keys
    size_t collisionTotalCnt = 0; // total count of keys across all buckets with 2 or more keys
    size_t chainLengthMax = 0; // maximum number of keys in a single bucket chain
    double chainLengthAvg = 0.0; // average number of keys in buckets with 2 or more keys
  };

  // Node structure for separate chaining. Allocated by AllocatorTrivial.
  struct Node {
    KeyType key;
    PayloadType payload;
    Node* next; // Pointer to the next node in the chain

    // Constructor for placement new. Note: KeyType and PayloadType must be trivially copyable
    // or have appropriate constructors if they are more complex.
    Node(const KeyType& k, const PayloadType& p, Node* n = nullptr) 
      : key(k), payload(p), next(n) {}

    // No explicit destructor needed for POD/trivial types, or if their destructors are called explicitly.
  };

  // Use AllocatorTrivial to manage Node objects
  using NodeTypeAllocator = AllocatorTrivial<Node>;

  explicit HashTableTrivial(size_t initialKeyCount, size_t numBuckets = 0) 
    : _num_buckets(numBuckets),
      _node_allocator(initialKeyCount) // Pre-allocate for initialKeyCount nodes
  {
      if (initialKeyCount == 0) {
          throw std::invalid_argument("initialKeyCount must be greater than 0.");
      }
      // Derive number of buckets if not provided, ensuring a reasonable load factor
      if (_num_buckets == 0) {
          // Aim for a load factor of around 0.7, then round to next power of 2
          size_t suggested_buckets = static_cast<size_t>(std::ceil(static_cast<double>(initialKeyCount) / 0.7));
          _num_buckets = std::bit_ceil(suggested_buckets);
          // Ensure minimum number of buckets, e.g., 8 or 16 for small tables
          if (_num_buckets < 8) _num_buckets = 8;
      }
      
      _buckets.resize(_num_buckets, nullptr); // Initialize all bucket heads to nullptr
  }

  ~HashTableTrivial() {
      clear(); // Clean up all allocated nodes
  }

  // Deleted copy/move constructors and assignment operators
  HashTableTrivial(const HashTableTrivial&) = delete;
  HashTableTrivial& operator=(const HashTableTrivial&) = delete;
  HashTableTrivial(HashTableTrivial&&) = delete;
  HashTableTrivial& operator=(HashTableTrivial&&) = delete;

  // Insert a key-payload pair.
  // Returns true if inserted, false if key already exists.
  bool insert(const KeyType& key, const PayloadType& payload) {
      size_t bucket_idx = _hash_func(key) % _num_buckets;
      Node* current = _buckets[bucket_idx];

      // Check for existing key
      while (current != nullptr) {
          if (_key_equal(current->key, key)) {
              return false; // Key already exists
          }
          current = current->next;
      }

      // Key not found, create new node
      // Allocate raw memory for the node using AllocatorTrivial
      Node* new_node_mem = _node_allocator.allocate();
      // Construct the Node in the allocated memory
      _node_allocator.construct(new_node_mem, key, payload, _buckets[bucket_idx]);

      _buckets[bucket_idx] = new_node_mem; // Add to head of chain
      _size++;
      return true;
  }

  // Find a key and return its payload, or default/sentinel if not found.
  // Returns a pointer to the payload, or nullptr if not found.
  PayloadType* find(const KeyType& key) {
      size_t bucket_idx = _hash_func(key) % _num_buckets;
      Node* current = _buckets[bucket_idx];

      while (current != nullptr) {
          if (_key_equal(current->key, key)) {
              return &(current->payload);
          }
          current = current->next;
      }
      return nullptr; // Key not found
  }

  // Find a key and return its payload (const version).
  const PayloadType* find(const KeyType& key) const {
      size_t bucket_idx = _hash_func(key) % _num_buckets;
      Node* current = _buckets[bucket_idx];

      while (current != nullptr) {
          if (_key_equal(current->key, key)) {
              return &(current->payload);
          }
          current = current->next;
      }
      return nullptr; // Key not found
  }

  // Erase a key.
  // Returns true if erased, false if key not found.
  bool erase(const KeyType& key) {
      size_t bucket_idx = _hash_func(key) % _num_buckets;
      Node* current = _buckets[bucket_idx];
      Node* prev = nullptr;

      while (current != nullptr) {
          if (_key_equal(current->key, key)) {
              if (prev == nullptr) {
                  _buckets[bucket_idx] = current->next; // Remove from head
              } else {
                  prev->next = current->next; // Remove from middle/tail
              }
              
              // Explicitly destroy the Node object and then deallocate its memory
              _node_allocator.destroy(current); // Call destructor
              _node_allocator.free(current); // Deallocate memory using AllocatorTrivial
              _size--;
              return true;
          }
          prev = current;
          current = current->next;
      }
      return false; // Key not found
  }

  // Clear all elements from the hash table.
  void clear() {
      for (size_t i = 0; i < _num_buckets; ++i) {
          Node* current = _buckets[i];
          while (current != nullptr) {
              Node* next = current->next;
              _node_allocator.destroy(current); // Destroy the Node object
              _node_allocator.free(current); // Deallocate memory
              current = next;
          }
          _buckets[i] = nullptr; // Reset bucket head
      }
      _size = 0;
  }

  // Get current number of elements.
  size_t size() const noexcept {
      return _size;
  }

  // Check if the hash table is empty.
  bool empty() const noexcept {
      return _size == 0;
  }

  // Populate KeyDistribution structure with current hash table statistics.
  void distribution(KeyDistribution& dist) const noexcept {
      dist.bucketCnt = _num_buckets;
      dist.keyCnt = _size;
      dist.bucketUsedCnt = 0;
      dist.collisionCnt = 0;
      dist.collisionTotalCnt = 0;
      dist.chainLengthMax = 0;

      size_t total_collision_keys = 0;
      size_t collision_buckets_count = 0;

      for (size_t i = 0; i < _num_buckets; ++i) {
          size_t chain_length = 0;
          Node* current = _buckets[i];
          while (current != nullptr) {
              chain_length++;
              current = current->next;
          }

          if (chain_length > 0) {
              dist.bucketUsedCnt++;
          }

          if (chain_length >= 2) {
              dist.collisionCnt++;
              total_collision_keys += chain_length;
              dist.chainLengthMax = std::max(dist.chainLengthMax, chain_length);
              collision_buckets_count++;
          }
      }
      dist.collisionTotalCnt = total_collision_keys;
      dist.chainLengthAvg = (collision_buckets_count > 0) 
                            ? static_cast<double>(total_collision_keys) / collision_buckets_count 
                            : 0.0;
  }

private:
  size_t _num_buckets;
  std::vector<Node*> _buckets; // Array of linked list heads (managed by std::vector, which may use heap)
  NodeTypeAllocator _node_allocator; // Allocator to manage Node objects without runtime heap calls
  Hash _hash_func; // Hash function for KeyType
  KeyEqual _key_equal; // Equality predicate for KeyType
  size_t _size = 0; // Current number of elements in the table

  // Private helper to wrap AllocatorTrivial::allocate for this class's Node type
  Node* allocate_node_from_pool() {
      return _node_allocator.allocate();
  }

  // Private helper to wrap AllocatorTrivial::free for this class's Node type
  void free_node_to_pool(Node* node) {
      _node_allocator.free(node);
  }
};

} // namespace hw::utility
