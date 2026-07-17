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

#ifndef SOMEIP_MUTEX_HPP
#define SOMEIP_MUTEX_HPP

// RAII owner of a platform mutex, lock it with someIp::Lock

#ifdef SOMEIP_PLATFORM_STM32
#include "lwip/sys.h"
#include "lwip/debug.h"
#else
#include <mutex>
#endif

namespace someIp {

#ifdef SOMEIP_PLATFORM_STM32

class Mutex {
public:
  // creation can fail on RTOS kernel-object pool exhaustion (report M-15)
  Mutex() { LWIP_ASSERT("sys_mutex_new failed", sys_mutex_new(&m_ptr_mutex) == ERR_OK); }
  ~Mutex() { sys_mutex_free(&m_ptr_mutex); }

  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;

  sys_mutex_t &native() { return m_ptr_mutex; }

private:
  sys_mutex_t m_ptr_mutex;
};

#else // POSIX host

class Mutex {
public:
  Mutex() = default;
  ~Mutex() = default;

  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;

  std::recursive_mutex &native() { return m_mutex; }

private:
  std::recursive_mutex m_mutex;
};

#endif

} // namespace someIp

#endif // SOMEIP_MUTEX_HPP
