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

#ifndef RTPS_THREADSAFECIRCULARBUFFER_TPP
#define RTPS_THREADSAFECIRCULARBUFFER_TPP

#include "someIp/os/Os.hpp"
#include "ThreadSafeCircularBuffer.hpp"
#include "someIp/config/logging_config.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char CIRCULAR_BUFFER_TAG[] = "CIRCULAR_BUFFER";
using CIRCULAR_BUFFER_LOGGER = BaseLogger<CIRCULAR_BUFFER_TAG>;

namespace someIp {

template <typename T, uint16_t SIZE>
bool ThreadSafeCircularBuffer<T, SIZE>::init() {
  m_initialized = true;
  return true;
}

template <typename T, uint16_t SIZE>
ThreadSafeCircularBuffer<T, SIZE>::~ThreadSafeCircularBuffer() = default;

template <typename T, uint16_t SIZE>
bool ThreadSafeCircularBuffer<T, SIZE>::moveElementIntoBuffer(T &&ref_elem) {
  os::Guard lock(m_mutex);
  if (!isFull()) {
    m_buffer[m_head] = std::move(ref_elem);
    incrementHead();
    return true;
  }
  return false;
}

template <typename T, uint16_t SIZE>
T ThreadSafeCircularBuffer<T, SIZE>::moveFirstInto(bool *ptr_result) {
  os::Guard lock(m_mutex);
  if (m_head != m_tail) {
    T hull = std::move(m_buffer[m_tail]);
    incrementTail();
    if (ptr_result) {
      *ptr_result = true;
    }
    return hull;
  }

  if (ptr_result) {
    *ptr_result = false;
  }
  return T();
}

template <typename T, uint16_t SIZE>
void ThreadSafeCircularBuffer<T, SIZE>::clear() {
  os::Guard lock(m_mutex);
  m_head = m_tail;
}

template <typename T, uint16_t SIZE>
bool ThreadSafeCircularBuffer<T, SIZE>::isFull() {
  auto it = m_head;
  incrementIterator(it);
  return it == m_tail;
}

template <typename T, uint16_t SIZE>
inline void
ThreadSafeCircularBuffer<T, SIZE>::incrementIterator(uint16_t &ref_iterator) {
  ++ref_iterator;
  if (ref_iterator >= m_buffer.size()) {
    ref_iterator = 0;
  }
}

template <typename T, uint16_t SIZE>
inline void ThreadSafeCircularBuffer<T, SIZE>::incrementTail() {
  incrementIterator(m_tail);
}

template <typename T, uint16_t SIZE>
inline void ThreadSafeCircularBuffer<T, SIZE>::incrementHead() {
  incrementIterator(m_head);
  // head catching tail drops the oldest element
  if (m_head == m_tail) {
    incrementTail();
  }
}
} // namespace someIp

#endif // RTPS_THREADSAFECIRCULARBUFFER_TPP
