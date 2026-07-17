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

#ifndef SOMEIP_UTILS_WIREBOUNDS_HPP
#define SOMEIP_UTILS_WIREBOUNDS_HPP

#include <cstddef>
#include <cstdint>

#ifdef SOMEIP_PLATFORM_STM32
#include "lwip/pbuf.h"
#endif

// bounds math in uint64_t to avoid size_t wrap
namespace someIp {
namespace wire {

// true if [offset, offset+len) fits within max
inline bool within_bound(uint32_t offset, uint32_t len, uint32_t max) {
  return static_cast<uint64_t>(offset) + static_cast<uint64_t>(len) <=
         static_cast<uint64_t>(max);
}

// full message is someip_length + 8, capped at 0xFFFF
inline bool valid_someip_length(uint32_t someip_length, uint32_t max_msg) {
  const uint64_t total = static_cast<uint64_t>(someip_length) + 8u;
  if (total > 0xFFFFu) return false;
  return total <= static_cast<uint64_t>(max_msg);
}

#ifdef SOMEIP_PLATFORM_STM32
// true if the first pbuf segment alone holds need bytes
inline bool first_seg_has(const pbuf *ptr_p, uint16_t need) {
  return ptr_p != nullptr && ptr_p->len >= need;
}
#endif

} // namespace wire
} // namespace someIp

#endif // SOMEIP_UTILS_WIREBOUNDS_HPP
