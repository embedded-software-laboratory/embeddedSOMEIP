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

#ifndef SOMEIP_UTILS_STATICQUEUE_HPP
#define SOMEIP_UTILS_STATICQUEUE_HPP

#include <cstddef>
#include <utility>

namespace someIp {

// fixed-capacity FIFO ring, push returns false when full
template <class T, size_t N>
class StaticQueue {
public:
  bool push(const T &ref_v) {
    if (m_count >= N) return false;
    m_data[m_tail] = ref_v;
    m_tail = (m_tail + 1) % N;
    ++m_count;
    return true;
  }
  bool push(T &&ref_v) {
    if (m_count >= N) return false;
    m_data[m_tail] = std::move(ref_v);
    m_tail = (m_tail + 1) % N;
    ++m_count;
    return true;
  }

  T &front() { return m_data[m_head]; }
  const T &front() const { return m_data[m_head]; }

  void pop() {
    if (m_count == 0) return;
    m_data[m_head] = T{};
    m_head = (m_head + 1) % N;
    --m_count;
  }

  bool empty() const { return m_count == 0; }
  bool full() const { return m_count == N; }
  size_t size() const { return m_count; }
  void clear() {
    while (!empty()) pop();
  }

private:
  T m_data[N] = {};
  size_t m_head = 0;
  size_t m_tail = 0;
  size_t m_count = 0;
};

} // namespace someIp

#endif // SOMEIP_UTILS_STATICQUEUE_HPP
