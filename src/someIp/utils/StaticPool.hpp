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

#ifndef SOMEIP_UTILS_STATICPOOL_HPP
#define SOMEIP_UTILS_STATICPOOL_HPP

#include <cstddef>
#include <new>
#include <utility>

#include "someIp/utils/CapacityCheck.hpp"

namespace someIp {

// no-heap object pool, works for non-movable types
template <class T, size_t N>
class StaticPool {
public:
  StaticPool() = default;
  ~StaticPool() {
    for (size_t i = 0; i < N; ++i)
      if (m_used[i]) reinterpret_cast<T *>(&m_slots[i])->~T();
  }
  StaticPool(const StaticPool &) = delete;
  StaticPool &operator=(const StaticPool &) = delete;

  template <class... Args>
  T *acquire(Args &&...args) {
    for (size_t i = 0; i < N; ++i) {
      if (!m_used[i]) {
        m_used[i] = true;
        return ::new (&m_slots[i]) T(std::forward<Args>(args)...);
      }
    }
    capacity_exhausted("StaticPool");
    return nullptr;
  }

  void release(T *ptr_p) {
    for (size_t i = 0; i < N; ++i) {
      if (m_used[i] && reinterpret_cast<T *>(&m_slots[i]) == ptr_p) {
        ptr_p->~T();
        m_used[i] = false;
        return;
      }
    }
  }

private:
  alignas(T) unsigned char m_slots[N][sizeof(T)] = {};
  bool m_used[N] = {};
};

} // namespace someIp

#endif // SOMEIP_UTILS_STATICPOOL_HPP
