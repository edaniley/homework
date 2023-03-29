#pragma once

#include <memory>
#include <mutex>
#include <list>
#include <condition_variable>


namespace hw {
// load with 'consume' (data-dependent) memory ordering
template<typename T>
T LoadConsume(T const* addr) {
    // hardware fence is implicit on x86
    T v = *const_cast<T const volatile*>(addr);
    //__memory_barrier(); // compiler fence
    asm volatile("": : :"memory");
    return v;
}

// store with 'release' memory ordering
template<typename T>
void StoreRelease(T* addr, T v) {
    // hardware fence is implicit on x86
    //__memory_barrier(); // compiler fence
    asm volatile("": : :"memory");
    *const_cast<T volatile*>(addr) = v;
}

// cache line size on modern x86 processors (in bytes)
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif
// single-producer/single-consumer queue
template<typename T>
class SPSCQueue {
public:
  SPSCQueue() {
      node* n = new node;
      n->m_next = 0;
      m_head = m_tail = m_first= m_head_copy = n;
  }

  ~SPSCQueue() {
      node* n = m_first;
      do {
          node* next = n->m_next;
          delete n;
          n = next;
      }
      while (n);
  }

  void Add(T v) {
      node* n = alloc_node();
      n->m_next = 0;
      n->m_value = v;
      StoreRelease(&m_tail->m_next, n);
      m_tail = n;
  }

  // returns 'false' if queue is empty
  bool Remove(T& v) {
      if (LoadConsume(&m_head->m_next)) {
          v = m_head->m_next->m_value;
          StoreRelease(&m_head, m_head->m_next);
          return true;
      }
      else {
          return false;
      }
  }

private:
  // internal node structure
  struct node {
      node* m_next;
      T m_value;
  };

  // consumer part
  // accessed mainly by consumer, infrequently be producer
  node* m_head; 			// head of the queue

  // delimiter between consumer part and producer part,
  // so that they situated on different cache lines
  char cache_line_pad_ [CACHE_LINE_SIZE];

  // producer part
  // accessed only by producer
  node* m_tail; 			// tail of the queue
  node* m_first; 		  // last unused node (head of node cache)
  node* m_head_copy; 	// helper (points somewhere between m_first and m_head)

  node* alloc_node() {
      // first tries to allocate node from internal node cache,
      // if attempt fails, allocates node via ::operator new()
      if (m_first != m_head_copy)       {
          node* n = m_first;
          m_first = m_first->m_next;
          return n;
      }

      m_head_copy = LoadConsume(&m_head);

      if (m_first != m_head_copy)       {
          node* n = m_first;
          m_first = m_first->m_next;
          return n;
      }

      node* n = new node;
      return n;
  }

  SPSCQueue(SPSCQueue const&);
  SPSCQueue& operator = (SPSCQueue const&);
};

// usage example

inline
void test_spsc_queue() {

  SPSCQueue<int> q;

  q.Add(1);
  q.Add(2);

  int v;

  bool b = q.Remove(v);(void) b;

  b = q.Remove(v);
  b = q.Remove(v);

  q.Add(3);
  q.Add(4);
  q.Add(5);

  b = q.Remove(v);
  b = q.Remove(v);
  b = q.Remove(v);
}

////////////////////// simple producer-consumer queue /////////////////////////

template <typename T>
class ProducerConsumerQueue {
public:
  void Enqueue(std::shared_ptr<T> obj) {
    std::scoped_lock lock(mtx_);
    que_.push_back(obj);
    cv_.notify_one();
  }

  std::shared_ptr<T> Dequeue() {
    std::unique_lock lock(mtx_);
    while (que_.size() == 0) {
      cv_.wait(lock);
    }
    std::shared_ptr<T> obj = que_.front();
    que_.pop_front();
    return obj;
  }

  size_t Purge() {
    std::scoped_lock lock(mtx_);
    const size_t purged = Size();
    que_.clear();
    return purged;
  }

  size_t Size() const { return que_.size(); }
  bool Empty() const { return Size() == 0; }

private:
  std::mutex                    mtx_;
  std::condition_variable       cv_;
  std::list<std::shared_ptr<T>> que_;
};

}
