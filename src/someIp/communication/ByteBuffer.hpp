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

#ifndef SOMEIP_COMMUNICATION_BYTEBUFFER_HPP
#define SOMEIP_COMMUNICATION_BYTEBUFFER_HPP

#include <cstdint>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "someIp/config/StackConfig.hpp"

namespace someIp {

// move-only byte buffer with inline storage, no heap
class ByteBuffer {
public:
  struct VTable {
    const uint8_t *(*data)(void *ptr_self);          // contiguous read view, may flatten
    size_t (*size)(const void *ptr_self);
    void *(*release_native)(void *ptr_self);         // detach+return native handle, or nullptr
    void (*move)(void *ptr_src, void *ptr_dst);          // move-construct dst, destroy src
    void (*destroy)(void *ptr_self);
  };

  ByteBuffer() = default;
  ByteBuffer(const ByteBuffer &) = delete;
  ByteBuffer &operator=(const ByteBuffer &) = delete;

  ByteBuffer(ByteBuffer &&ref_o) noexcept {
    if (ref_o.m_ptr_vt) {
      ref_o.m_ptr_vt->move(&ref_o.m_storage, &m_storage);
      m_ptr_vt = ref_o.m_ptr_vt;
      ref_o.m_ptr_vt = nullptr;
    }
  }
  ByteBuffer &operator=(ByteBuffer &&ref_o) noexcept {
    if (this != &ref_o) {
      reset();
      if (ref_o.m_ptr_vt) {
        ref_o.m_ptr_vt->move(&ref_o.m_storage, &m_storage);
        m_ptr_vt = ref_o.m_ptr_vt;
        ref_o.m_ptr_vt = nullptr;
      }
    }
    return *this;
  }
  ~ByteBuffer() { reset(); }

  const uint8_t *data() { return m_ptr_vt ? m_ptr_vt->data(&m_storage) : nullptr; }
  size_t size() const { return m_ptr_vt ? m_ptr_vt->size(&m_storage) : 0; }
  void *release_native() { return m_ptr_vt ? m_ptr_vt->release_native(&m_storage) : nullptr; }
  explicit operator bool() const noexcept { return m_ptr_vt != nullptr; }

  void reset() noexcept {
    if (m_ptr_vt) {
      m_ptr_vt->destroy(&m_storage);
      m_ptr_vt = nullptr;
    }
  }

  template <class Impl, class... Args>
  static ByteBuffer create(const VTable *ptr_vt, Args &&...args) {
    static_assert(sizeof(Impl) <= config::BUFFER_INPLACE_SIZE,
                  "ByteBuffer backend too large (raise BUFFER_INPLACE_SIZE)");
    static_assert(alignof(Impl) <= alignof(std::max_align_t), "ByteBuffer backend over-aligned");
    ByteBuffer b;
    ::new (static_cast<void *>(&b.m_storage)) Impl(std::forward<Args>(args)...);
    b.m_ptr_vt = ptr_vt;
    return b;
  }

  // public so backend headers can build their vtable
  template <class Impl> static const uint8_t *data_thunk(void *ptr_s) { return static_cast<Impl *>(ptr_s)->data(); }
  template <class Impl> static size_t size_thunk(const void *ptr_s) { return static_cast<const Impl *>(ptr_s)->size(); }
  template <class Impl> static void *release_native_thunk(void *ptr_s) { return static_cast<Impl *>(ptr_s)->release_native(); }
  template <class Impl> static void move_thunk(void *ptr_src, void *ptr_dst) {
    ::new (ptr_dst) Impl(std::move(*static_cast<Impl *>(ptr_src)));
    static_cast<Impl *>(ptr_src)->~Impl();
  }
  template <class Impl> static void destroy_thunk(void *ptr_s) { static_cast<Impl *>(ptr_s)->~Impl(); }

private:
  using Storage = std::aligned_storage_t<config::BUFFER_INPLACE_SIZE, alignof(std::max_align_t)>;
  Storage m_storage;
  const VTable *m_ptr_vt = nullptr;
};

template <class Impl>
inline const ByteBuffer::VTable *byte_buffer_vtable() {
  static const ByteBuffer::VTable vt{
      &ByteBuffer::data_thunk<Impl>, &ByteBuffer::size_thunk<Impl>,
      &ByteBuffer::release_native_thunk<Impl>, &ByteBuffer::move_thunk<Impl>,
      &ByteBuffer::destroy_thunk<Impl>};
  return &vt;
}

// std::vector backed backend for the POSIX transport
class VectorByteBuffer {
public:
  VectorByteBuffer() = default;
  explicit VectorByteBuffer(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes)) {}

  const uint8_t *data() { return m_bytes.empty() ? nullptr : m_bytes.data(); }
  size_t size() const { return m_bytes.size(); }
  void *release_native() { return nullptr; } // not pbuf-backed

private:
  std::vector<uint8_t> m_bytes;
};

using BufferPtr = ByteBuffer; // historical name

inline ByteBuffer make_vector_buffer(std::vector<uint8_t> bytes) {
  return ByteBuffer::create<VectorByteBuffer>(byte_buffer_vtable<VectorByteBuffer>(), std::move(bytes));
}

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_BYTEBUFFER_HPP
