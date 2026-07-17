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

#ifndef SOMEIP_COMMUNICATION_PBUFBYTEBUFFER_HPP
#define SOMEIP_COMMUNICATION_PBUFBYTEBUFFER_HPP

#include <vector>
#include <utility>

#include <lwip/pbuf.h>

#include "someIp/communication/ByteBuffer.hpp"
#include "someIp/structs/PbufWrapper.hpp"

namespace someIp {

// lwIP pbuf backed ByteBuffer backend, stored inline, flattens lazily
class PbufByteBuffer {
public:
  explicit PbufByteBuffer(PbufWrapper &&ref_wrapper) : m_wrapper(std::move(ref_wrapper)) {}

  const uint8_t *data() {
    pbuf *ptr_p = m_wrapper.get();
    if (!ptr_p || ptr_p->tot_len == 0) {
      return nullptr;
    }
    // contiguous pbuf, no flatten needed
    if (ptr_p->len == ptr_p->tot_len) {
      return static_cast<const uint8_t *>(ptr_p->payload);
    }
    // chained pbuf, flatten once into the cache
    flatten();
    return m_flat.empty() ? nullptr : m_flat.data();
  }
  size_t size() const {
    pbuf *ptr_p = m_wrapper.get();
    return ptr_p ? ptr_p->tot_len : 0;
  }
  // detach the owned pbuf for zero-copy transmit, wrapper becomes empty
  void *release_native() { return m_wrapper.detach(); }

private:
  void flatten() {
    if (m_flattened) {
      return;
    }
    pbuf *ptr_p = m_wrapper.get();
    if (ptr_p && ptr_p->tot_len > 0) {
      m_flat.resize(ptr_p->tot_len);
      pbuf_copy_partial(ptr_p, m_flat.data(), ptr_p->tot_len, 0);
    }
    m_flattened = true;
  }

  PbufWrapper m_wrapper;
  std::vector<uint8_t> m_flat;
  bool m_flattened = false;
};

inline ByteBuffer make_pbuf_buffer(PbufWrapper &&ref_wrapper) {
  return ByteBuffer::create<PbufByteBuffer>(byte_buffer_vtable<PbufByteBuffer>(), std::move(ref_wrapper));
}

// recover an owning PbufWrapper from any ByteBuffer, zero-copy when pbuf-backed
inline PbufWrapper take_pbuf(ByteBuffer buf) {
  if (void *ptr_h = buf.release_native()) {
    return PbufWrapper(static_cast<pbuf *>(ptr_h));
  }
  // not pbuf-backed, allocate a pbuf and copy the bytes in
  const size_t n = buf.size();
  const uint8_t *ptr_src = buf.data();
  if (n == 0 || ptr_src == nullptr) {
    return PbufWrapper();
  }
  pbuf *ptr_p = pbuf_alloc(PBUF_RAW, static_cast<u16_t>(n), PBUF_RAM);
  if (!ptr_p) {
    return PbufWrapper();
  }
  pbuf_take(ptr_p, ptr_src, static_cast<u16_t>(n));
  return PbufWrapper(ptr_p);
}

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_PBUFBYTEBUFFER_HPP
