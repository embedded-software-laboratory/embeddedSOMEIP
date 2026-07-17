/*
The MIT License
Copyright (c) 2026 Lehrstuhl Informatik 11 - RWTH Aachen University
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE

This file is part of embeddedSOMEIP.

Author: i11 - Embedded Software, RWTH Aachen University
*/

#ifndef SOMEIP_OS_OS_HPP
#define SOMEIP_OS_OS_HPP

#include <cstdint>

#include "someIp/utils/Mutex.hpp"
#include "someIp/utils/Lock.hpp"

// OS seam, lwIP sys_arch on STM32 and std threads on POSIX

#ifdef SOMEIP_PLATFORM_STM32
#include "lwip/sys.h"
#else
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#endif

namespace someIp {
namespace os {

using Mutex = someIp::Mutex;
using Lock = someIp::Lock;

// scoped lock over an os::Mutex
class Guard {
public:
  explicit Guard(Mutex &ref_m) : m_lock(ref_m.native()) {}
  Guard(const Guard &) = delete;
  Guard &operator=(const Guard &) = delete;

private:
  Lock m_lock;
};

using ThreadFn = void (*)(void *);

#ifdef SOMEIP_PLATFORM_STM32

// counting/binary semaphore
class Semaphore {
public:
  // creation can fail on kernel-object pool exhaustion (report M-15)
  explicit Semaphore(uint8_t count = 0) {
    LWIP_ASSERT("sys_sem_new failed", sys_sem_new(&m_ptr_sem, count) == ERR_OK);
  }
  ~Semaphore() { sys_sem_free(&m_ptr_sem); }
  Semaphore(const Semaphore &) = delete;
  Semaphore &operator=(const Semaphore &) = delete;

  void wait() { sys_arch_sem_wait(&m_ptr_sem, 0); }
  void signal() { sys_sem_signal(&m_ptr_sem); }
  sys_sem_t &native() { return m_ptr_sem; }

private:
  sys_sem_t m_ptr_sem;
};

inline void sleep_ms(uint32_t ms) { sys_msleep(ms); }
inline uint32_t now_ms() { return sys_now(); }

// lwIP threads are not joinable
using ThreadHandle = sys_thread_t;
inline ThreadHandle spawn(const char *ptr_name, ThreadFn ptr_fn, void *ptr_arg, int stack_size, int prio) {
  return sys_thread_new(ptr_name, ptr_fn, ptr_arg, stack_size, prio);
}

#else // POSIX host

// mutex and condvar, C++17 has no counting_semaphore
class Semaphore {
public:
  explicit Semaphore(uint8_t count = 0) : count_(count) {}
  Semaphore(const Semaphore &) = delete;
  Semaphore &operator=(const Semaphore &) = delete;

  void wait() {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [this] { return count_ > 0; });
    --count_;
  }
  void signal() {
    std::lock_guard<std::mutex> lk(m_);
    ++count_;
    cv_.notify_one();
  }

private:
  std::mutex m_;
  std::condition_variable cv_;
  unsigned count_;
};

inline void sleep_ms(uint32_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline uint32_t now_ms() {
  static const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count());
}

// detached, threads are never joined
using ThreadHandle = void *;
inline ThreadHandle spawn(const char *name, ThreadFn fn, void *arg, int stack_size, int prio) {
  (void)name;
  (void)stack_size;
  (void)prio;
  std::thread(fn, arg).detach();
  return nullptr;
}

#endif

} // namespace os
} // namespace someIp

#endif // SOMEIP_OS_OS_HPP
