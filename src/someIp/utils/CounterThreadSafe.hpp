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

#ifndef SOMEIP_COUNTER_THREAD_SAFE_HPP
#define SOMEIP_COUNTER_THREAD_SAFE_HPP

#include <cstddef>

#include "someIp/os/Os.hpp"

class CounterThreadSafe {
public:
  CounterThreadSafe() = default;

  void count_up() {
    someIp::os::Guard g(m_mutex);
    m_value++;
  }

  void count_down() {
    someIp::os::Guard g(m_mutex);
    if (m_value > 0) m_value--; // guard size_t underflow (report C-7/m-15)
  }

  size_t get_value() {
    someIp::os::Guard g(m_mutex);
    return m_value;
  }

  // wait until counter hits zero or timeout (poll iterations of 10ms) elapses
  bool wait_until_zero(size_t timeout = 100) {
    while (timeout != 0) {
      {
        someIp::os::Guard g(m_mutex);
        if (m_value == 0) {
          return true;
        }
      }
      someIp::os::sleep_ms(10);
      timeout--;
    }
    someIp::os::Guard g(m_mutex);
    return m_value == 0;
  }

private:
  someIp::os::Mutex m_mutex;
  size_t m_value = 0;
};

#endif //SOMEIP_COUNTER_THREAD_SAFE_HPP
