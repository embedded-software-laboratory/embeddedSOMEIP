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

#ifndef SOMEIP_UTILS_STATICMAP_HPP
#define SOMEIP_UTILS_STATICMAP_HPP

#include <cstddef>
#include <utility>

#include "someIp/utils/CapacityCheck.hpp"

namespace someIp {

// no-heap flat map, operator[] returns a scratch slot when full
template <class K, class V, size_t N>
class StaticMap {
public:
  // first/second to match std::map iterator usage
  struct Pair {
    K first{};
    V second{};
  };
  using iterator = Pair *;
  using const_iterator = const Pair *;

  iterator find(const K &ref_k) {
    for (size_t i = 0; i < m_size; ++i)
      if (m_data[i].first == ref_k) return &m_data[i];
    return end();
  }
  const_iterator find(const K &ref_k) const {
    for (size_t i = 0; i < m_size; ++i)
      if (m_data[i].first == ref_k) return &m_data[i];
    return end();
  }

  V &operator[](const K &ref_k) {
    iterator ptr_it = find(ref_k);
    if (ptr_it != end()) return ptr_it->second;
    if (m_size < N) {
      m_data[m_size].first = ref_k;
      m_data[m_size].second = V{};
      return m_data[m_size++].second;
    }
    SOMEIP_CAPACITY_FAIL("StaticMap::operator[]");
    m_overflow = Pair{};
    return m_overflow.second;
  }

  size_t erase(const K &ref_k) {
    iterator ptr_it = find(ref_k);
    if (ptr_it == end()) return 0;
    erase(ptr_it);
    return 1;
  }

  iterator erase(iterator ptr_pos) {
    for (iterator ptr_p = ptr_pos; ptr_p + 1 != end(); ++ptr_p) *ptr_p = std::move(*(ptr_p + 1));
    m_data[--m_size] = Pair{};
    return ptr_pos;
  }

  size_t count(const K &ref_k) const { return find(ref_k) != end() ? 1 : 0; }
  size_t size() const { return m_size; }
  bool empty() const { return m_size == 0; }
  bool full() const { return m_size == N; }
  void clear() {
    for (size_t i = 0; i < m_size; ++i) m_data[i] = Pair{};
    m_size = 0;
  }

  iterator begin() { return m_data; }
  iterator end() { return m_data + m_size; }
  const_iterator begin() const { return m_data; }
  const_iterator end() const { return m_data + m_size; }

private:
  Pair m_data[N] = {};
  size_t m_size = 0;
  Pair m_overflow{};
};

} // namespace someIp

#endif // SOMEIP_UTILS_STATICMAP_HPP
