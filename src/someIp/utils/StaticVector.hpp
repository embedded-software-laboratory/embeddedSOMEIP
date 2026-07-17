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

#ifndef SOMEIP_UTILS_STATICVECTOR_HPP
#define SOMEIP_UTILS_STATICVECTOR_HPP

#include <cstddef>
#include <utility>
#include <initializer_list>

#include "someIp/utils/CapacityCheck.hpp"

namespace someIp {

// fixed-capacity no-heap vector, push/emplace return false when full
template <class T, size_t N>
class StaticVector {
public:
  using value_type = T;
  using iterator = T *;
  using const_iterator = const T *;

  StaticVector() = default;
  StaticVector(std::initializer_list<T> init) {
    for (const T &ref_v : init) push_back(ref_v);
  }

  bool push_back(const T &ref_value) {
    if (m_size >= N) { SOMEIP_CAPACITY_FAIL("StaticVector::push_back"); return false; }
    m_data[m_size++] = ref_value;
    return true;
  }
  bool push_back(T &&ref_value) {
    if (m_size >= N) { SOMEIP_CAPACITY_FAIL("StaticVector::push_back"); return false; }
    m_data[m_size++] = std::move(ref_value);
    return true;
  }
  template <class... Args>
  bool emplace_back(Args &&...args) {
    if (m_size >= N) { SOMEIP_CAPACITY_FAIL("StaticVector::emplace_back"); return false; }
    m_data[m_size++] = T(std::forward<Args>(args)...);
    return true;
  }

  T &operator[](size_t i) { return m_data[i]; }
  const T &operator[](size_t i) const { return m_data[i]; }
  T &back() { return m_data[m_size - 1]; }
  const T &back() const { return m_data[m_size - 1]; }
  T &front() { return m_data[0]; }
  const T &front() const { return m_data[0]; }

  void pop_back() {
    if (m_size > 0) m_data[--m_size] = T{};
  }

  // erases [first,last) and returns first
  iterator erase(const_iterator ptr_first, const_iterator ptr_last) {
    iterator ptr_dst = begin() + (ptr_first - begin());
    iterator ptr_l = begin() + (ptr_last - begin());
    for (iterator ptr_src = ptr_l; ptr_src != end(); ++ptr_src, ++ptr_dst) *ptr_dst = std::move(*ptr_src);
    size_t new_size = static_cast<size_t>(ptr_dst - begin());
    for (size_t i = new_size; i < m_size; ++i) m_data[i] = T{};
    m_size = new_size;
    return begin() + (ptr_first - begin());
  }
  iterator erase(const_iterator ptr_pos) { return erase(ptr_pos, ptr_pos + 1); }
  void clear() {
    for (size_t i = 0; i < m_size; ++i) m_data[i] = T{};
    m_size = 0;
  }

  size_t size() const { return m_size; }
  static constexpr size_t capacity() { return N; }
  bool empty() const { return m_size == 0; }
  bool full() const { return m_size == N; }

  T *data() { return m_data; }
  const T *data() const { return m_data; }
  iterator begin() { return m_data; }
  iterator end() { return m_data + m_size; }
  const_iterator begin() const { return m_data; }
  const_iterator end() const { return m_data + m_size; }

private:
  T m_data[N] = {};
  size_t m_size = 0;
};

} // namespace someIp

#endif // SOMEIP_UTILS_STATICVECTOR_HPP
