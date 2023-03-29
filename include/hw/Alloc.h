#pragma once

#include <iostream>
#include <vector>
#include <stack>
#include <atomic>
#include <memory>
#include <mutex>

#include "Util.h"

namespace hw {

template<typename T, int N = 128>
class MemAllocator {
  struct Mem {
    T value;
    Mem* next;
    int id;
  };

  struct Heap {
    ~Heap() {
      std::cout << "free " << hw::TypeName<T>() << ' ' << allocated_.size()
                << " allocations by " << N << " elements" << std::endl;
      for (Mem* mem : allocated_) {
        Free(mem);
      };
    }

    Mem* Alloc() {
      Mem* retval;
      std::scoped_lock<Spinlock> lock(mtx_);
      if (LIKELY(!free_.empty())) {
        retval = free_.top();
        free_.pop();
      }
      else {
        retval = alloc_N();
      }
      return retval;
    }

    void Free(Mem* mem) {
      std::scoped_lock<Spinlock> lock(mtx_);
      free_.push(mem);
    }

    Mem* alloc_N() {
      Mem* head = (Mem*)malloc(N * sizeof(Mem));
      Mem* tail = head + (N - 1);
      for (Mem* mem = head; mem < tail; ++mem) {
        mem->next = mem + 1;
        mem->id = ++id;
      }
      tail->next = nullptr;
      tail->id = ++id;
      allocated_.push_back(head);
      return head;
    }

    hw::Spinlock      mtx_;
    std::vector<Mem*> allocated_;
    std::stack<Mem*>  free_;
    int               id = 0;
  };

  struct Deleter {
    Deleter(Mem* mem) : mem_(mem) {}
    void operator()(T* t) noexcept {
      t->~T();
      thrd_free(mem_);
    }
    Mem* mem_;
  };

public:
  using unique_ptr = std::unique_ptr<T, Deleter>;

  template <typename... Types>
  static auto make_unique(Types &&... Args) {
    Mem* mem = thrd_alloc();
    T* t = new (mem) T(std::forward<Types>(Args) ...);
    return std::unique_ptr<T, Deleter>(t, Deleter(mem));
  }

  template <typename... Types>
  static auto make_shared(Types &&... Args) {
    Mem* mem = thrd_alloc();
    T* t = new (mem) T(std::forward<Types>(Args) ...);
    return std::shared_ptr<T>(t, Deleter(mem));
  }

private:
  static Mem* thrd_alloc() {
    if (!thrd_free_cnt) {
      thrd_free_cnt = N;
      thrd_free_mem = heap_.Alloc();
    }
    Mem* retval = thrd_free_mem;
    thrd_free_mem = retval->next;
    --thrd_free_cnt;
    return retval;
  }
  static void thrd_free(Mem* mem) {
    if (thrd_free_cnt == N) {
      heap_.Free(thrd_free_mem);
      thrd_free_mem = nullptr;
      thrd_free_cnt = 0;

    }
    mem->next = thrd_free_mem;
    thrd_free_mem = mem;
    ++thrd_free_cnt;
  }

  inline static Heap heap_;
  inline static thread_local Mem* thrd_free_mem = nullptr;
  inline static thread_local size_t thrd_free_cnt = 0;
};

}

#ifdef HW_UNIT_TEST

#include <thread>
#include "Queue.h"

namespace  talloc_ {
struct TData {
  TData(int msgno) : m_msgno(msgno) {}
  int m_msgno;
};

using TQueue = hw::ProducerConsumerQueue<TData>;
using TAllocator = hw::MemAllocator<TData>;

static void Producer (int core, TAllocator & allocator, TQueue &queue, size_t msgcnt) {
  for (size_t msgno = 1; msgno <= msgcnt; ++msgno) {
    queue.Enqueue(std::move(allocator.make_unique(msgno)));
  }
  std::cout << "Producer core:" << core<< " msgcnt:" << msgcnt << std::endl;
}

static void Consumer (int core, TAllocator & allocator, TQueue &queue) {
  int no = 0;
  size_t msgcnt = 0;
  while (no != -1) {
    auto ptr = queue.Dequeue();
    ++msgcnt;
    no = ptr->m_msgno;
    ptr.reset();
  }
  std::cout << "Consumer core:" << core<< " msgcnt:" << msgcnt << std::endl;
}

}

inline
void test_Alloc(std::vector<int> cores = std::vector<int>{}) {
  using namespace talloc_;
  // use int as core
  TQueue queue;
  TAllocator allocator;

  if (cores.empty()) {
    cores = std::vector<int>{-1, -1};
  }
  auto ic = cores.begin();
  std::vector<std::thread> producers, consumers;
  const size_t producerCnt = cores.size() > 1 ? cores.size()/2 : 1;
  const size_t consumerCnt = cores.size() > 1 ? cores.size() - producerCnt : 1;
  for (size_t i = 0; i < consumerCnt; ++i) {
    consumers.emplace_back(Consumer, *ic ++, std::ref(allocator), std::ref(queue));
  }
  for (size_t i = 0; i < producerCnt; ++i) {
    producers.emplace_back(Producer, *ic ++, std::ref(allocator), std::ref(queue), 10000);
  }
  for (auto &thr : producers) {
    thr.join();
  }
  for (size_t i = 0; i < consumerCnt; ++i) {
    queue.Enqueue(std::move(allocator.make_unique(-1)));
  }
  for (auto &thr : consumers) {
    thr.join();
  }
}

#endif
