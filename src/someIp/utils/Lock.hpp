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
#ifndef RTPS_LOCK_H
#define RTPS_LOCK_H

// scoped lock over a someIp::Mutex::native() handle

#ifdef SOMEIP_PLATFORM_STM32
#include "lwip/sys.h"
#else
#include <mutex>
#endif

namespace someIp {

#ifdef SOMEIP_PLATFORM_STM32

class Lock {
public:
  explicit Lock(sys_mutex_t &ref_passedMutex) : m_ref_mutex(ref_passedMutex) {
    sys_mutex_lock(&m_ref_mutex);
  };

  ~Lock() { sys_mutex_unlock(&m_ref_mutex); };

  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;

private:
  sys_mutex_t &m_ref_mutex;
};

#else // POSIX host

class Lock {
public:
  explicit Lock(std::recursive_mutex &passedMutex) : m_mutex(passedMutex) {
    m_mutex.lock();
  };

  ~Lock() { m_mutex.unlock(); };

  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;

private:
  std::recursive_mutex &m_mutex;
};

#endif

} // namespace someIp
#endif // RTPS_LOCK_H
