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

#ifndef SOMEIP_PACKETBUFFER_HPP
#define SOMEIP_PACKETBUFFER_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <utility>

// the front cursor avoids copying the tail on strip and prepend

namespace someIp {

class PacketBuffer {
public:
  PacketBuffer() = default;
  explicit PacketBuffer(size_t n) : buf_(n) {}
  PacketBuffer(const void *src, size_t n)
      : buf_(static_cast<const uint8_t *>(src), static_cast<const uint8_t *>(src) + n) {}

  PacketBuffer(const PacketBuffer &) = delete;
  PacketBuffer &operator=(const PacketBuffer &) = delete;
  PacketBuffer(PacketBuffer &&) noexcept = default;
  PacketBuffer &operator=(PacketBuffer &&) noexcept = default;

  bool empty() const { return front_ >= buf_.size(); }
  explicit operator bool() const { return !empty(); }

  const uint8_t *data() const { return buf_.data() + front_; }
  uint8_t *data() { return buf_.data() + front_; }
  size_t size() const { return buf_.size() - front_; }

  // returns bytes copied, 0 on OOB
  size_t copy_out(void *dst, size_t n, size_t offset = 0) const {
    if (n == 0 || offset + n > size()) return 0;
    std::memcpy(dst, data() + offset, n);
    return n;
  }

  bool remove_front(size_t n) {
    if (n > size()) return false;
    front_ += n;
    return true;
  }

  void add_in_front(const void *hdr, size_t n) {
    const uint8_t *h = static_cast<const uint8_t *>(hdr);
    if (front_ >= n) { // reuse trimmed front space
      std::memcpy(buf_.data() + front_ - n, h, n);
      front_ -= n;
      return;
    }
    std::vector<uint8_t> nb;
    nb.reserve(n + size());
    nb.insert(nb.end(), h, h + n);
    nb.insert(nb.end(), data(), data() + size());
    buf_ = std::move(nb);
    front_ = 0;
  }

  void append(const void *src, size_t n) {
    const uint8_t *s = static_cast<const uint8_t *>(src);
    buf_.insert(buf_.end(), s, s + n);
  }

  // empty on OOB
  PacketBuffer slice_copy(size_t len, size_t offset) const {
    if (len == 0 || offset + len > size()) return PacketBuffer();
    return PacketBuffer(data() + offset, len);
  }

  // returns the first len bytes, keeps the remainder here
  PacketBuffer slice_move(size_t len) {
    if (len == 0 || len > size()) return PacketBuffer();
    PacketBuffer head(data(), len);
    front_ += len;
    return head;
  }

  // drops any trimmed front
  std::vector<uint8_t> take_bytes() {
    if (front_ > 0) {
      buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(front_));
      front_ = 0;
    }
    return std::move(buf_);
  }

private:
  std::vector<uint8_t> buf_;
  size_t front_ = 0;
};

} // namespace someIp

#endif // SOMEIP_PACKETBUFFER_HPP
