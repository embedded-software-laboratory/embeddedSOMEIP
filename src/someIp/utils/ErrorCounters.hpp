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

#ifndef SOMEIP_UTILS_ERRORCOUNTERS_HPP
#define SOMEIP_UTILS_ERRORCOUNTERS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "someIp/os/Os.hpp"

// no-heap drop accounting, member-owned to avoid static-init kernel objects
namespace someIp {

enum class DropCounter : uint8_t {
  rx_queue_full = 0,
  tx_queue_full,
  malformed_header,
  parse_drop,
  fsm_queue_full,
  reassembly_drop,
  response_table_full,
  callback_slot_leak,
  tcp_oom,
  udp_pool_exhausted,
  COUNT
};

class ErrorCounters {
public:
  void inc(DropCounter c) {
    os::Guard g(m_mtx);
    ++m_values[static_cast<size_t>(c)];
  }

  uint32_t get(DropCounter c) {
    os::Guard g(m_mtx);
    return m_values[static_cast<size_t>(c)];
  }

  std::array<uint32_t, static_cast<size_t>(DropCounter::COUNT)> snapshot() {
    os::Guard g(m_mtx);
    return m_values;
  }

private:
  os::Mutex m_mtx;
  std::array<uint32_t, static_cast<size_t>(DropCounter::COUNT)> m_values{};
};

// lazily constructed so no kernel object exists pre-main
inline ErrorCounters &drop_counters() {
  static ErrorCounters counters;
  return counters;
}

} // namespace someIp

#endif // SOMEIP_UTILS_ERRORCOUNTERS_HPP
