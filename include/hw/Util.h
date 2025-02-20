#pragma once

#include <pthread.h>
#include <cassert>
#include <iostream>
#include <sstream>
#include <memory>
#include <atomic>

#ifdef unix
#include <cxxabi.h>
#endif

#define LIKELY(x)    __builtin_expect (!!(x), 1)
#define UNLIKELY(x)  __builtin_expect (!!(x), 0)

#define CACHE_LINE_SIZE 64

namespace hw {

class Spinlock {
  std::atomic_flag locked = ATOMIC_FLAG_INIT;
public:
  void lock() {
    while (locked.test_and_set(std::memory_order_acquire)) { ; }
  }
  void unlock() {
    locked.clear(std::memory_order_release);
  }
};

inline void Abort(const char *errmsg, const char *errloc) {
  std::cerr << errloc << " [" << errmsg << "]" << std::endl;
}

#define HW_ABORT(MSG) Abort(MSG, __FILE_ ":" __LINE__);

//////////////////////// manage thread CPU affinity ///////////////////////////
#ifdef unix
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>

inline int SetThreadAffinity(int core) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

#else
#endif

}
