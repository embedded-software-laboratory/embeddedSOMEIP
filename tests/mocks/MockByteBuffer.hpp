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

#ifndef SOMEIP_TESTS_MOCKBYTEBUFFER_HPP
#define SOMEIP_TESTS_MOCKBYTEBUFFER_HPP

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>

#include "someIp/communication/ByteBuffer.hpp"
#include "someIp/utils/StaticPool.hpp"

namespace someIp {
namespace test {

// pooled blocks stand in for the lwIP pbuf pool
constexpr size_t MAX_MOCK_DATAGRAM = 1600;
constexpr size_t MOCK_DATAGRAM_POOL_BLOCKS = 64;

struct MockDatagramBlock {
  uint8_t data[MAX_MOCK_DATAGRAM];
};

inline StaticPool<MockDatagramBlock, MOCK_DATAGRAM_POOL_BLOCKS> &mock_datagram_pool() {
  static StaticPool<MockDatagramBlock, MOCK_DATAGRAM_POOL_BLOCKS> pool;
  return pool;
}

// StaticPool has no reset, so a leaked block would starve every later test
inline std::atomic<int> &mock_pool_live_blocks() {
  static std::atomic<int> live{0};
  return live;
}

class PooledByteBuffer {
public:
  PooledByteBuffer() = default;
  PooledByteBuffer(const uint8_t *d, size_t n) {
    block_ = mock_datagram_pool().acquire();
    if (block_) mock_pool_live_blocks().fetch_add(1);
    len_ = (n <= MAX_MOCK_DATAGRAM) ? n : MAX_MOCK_DATAGRAM;
    if (block_ && d && len_) std::memcpy(block_->data, d, len_);
  }
  PooledByteBuffer(const PooledByteBuffer &) = delete;
  PooledByteBuffer &operator=(const PooledByteBuffer &) = delete;
  PooledByteBuffer(PooledByteBuffer &&o) noexcept : block_(o.block_), len_(o.len_) {
    o.block_ = nullptr;
    o.len_ = 0;
  }
  ~PooledByteBuffer() {
    if (block_) {
      mock_datagram_pool().release(block_);
      mock_pool_live_blocks().fetch_sub(1);
    }
  }

  const uint8_t *data() { return block_ ? block_->data : nullptr; }
  size_t size() const { return len_; }
  void *release_native() { return nullptr; }

private:
  MockDatagramBlock *block_ = nullptr;
  size_t len_ = 0;
};

inline ByteBuffer make_pooled_buffer(const uint8_t *data, size_t len) {
  return ByteBuffer::create<PooledByteBuffer>(byte_buffer_vtable<PooledByteBuffer>(), data, len);
}

} // namespace test
} // namespace someIp

#endif // SOMEIP_TESTS_MOCKBYTEBUFFER_HPP
