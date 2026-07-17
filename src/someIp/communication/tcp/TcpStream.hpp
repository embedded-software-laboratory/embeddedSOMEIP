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

#ifndef SOMEIP_COMMUNICATION_TCP_TCPSTREAM_HPP
#define SOMEIP_COMMUNICATION_TCP_TCPSTREAM_HPP

#include <cstdint>
#include <lwip/pbuf.h>

namespace someIp {

// nullptr/0 signals the chain is exhausted
struct TcpChunk {
  const uint8_t *ptr;
  uint16_t len;
};

// next contiguous run from a pbuf chain, bounded by budget and segment
inline TcpChunk tcp_next_chunk(pbuf *&ref_p, uint16_t &ref_seg_off, uint32_t budget) {
  while (ref_p && ref_seg_off >= ref_p->len) {
    ref_p = ref_p->next;
    ref_seg_off = 0;
  }
  if (!ref_p || budget == 0) {
    return {nullptr, 0};
  }
  uint32_t seg_avail = static_cast<uint32_t>(ref_p->len) - ref_seg_off;
  uint16_t len = static_cast<uint16_t>(budget < seg_avail ? budget : seg_avail);
  return {static_cast<const uint8_t *>(ref_p->payload) + ref_seg_off, len};
}

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_TCP_TCPSTREAM_HPP
