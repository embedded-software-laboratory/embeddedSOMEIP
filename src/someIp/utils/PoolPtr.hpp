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

#ifndef SOMEIP_UTILS_POOLPTR_HPP
#define SOMEIP_UTILS_POOLPTR_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "someIp/utils/PoolTrace.hpp"

// pool-backed ref-counted smart pointer, no heap, made via POOL_MAKE
// last release destroys T and returns the block to its pool

namespace someIp {

// control header colocated with the object in one pool block
struct PoolCtrl {
  std::atomic<uint32_t> strong;
  uint16_t pool_id;                    // tracing only
  const char *alloc_site;              // tracing only
  void *pool;                          // type-erased owning FixedBlockPool
  void (*destroy_and_free)(PoolCtrl *);
};

// ctrl first so a PoolCtrl* round-trips back to the block base
template <class T>
struct PoolBlock {
  PoolCtrl ctrl;
  alignas(T) unsigned char obj[sizeof(T)];
};

// knows the concrete T so a PoolPtr<Base> still destroys the full object
template <class T, class Pool>
void pool_block_destroy(PoolCtrl *ptr_ctrl) {
  auto *ptr_blk = reinterpret_cast<PoolBlock<T> *>(ptr_ctrl);
  const uint16_t id = ptr_ctrl->pool_id;
  const char *ptr_site = ptr_ctrl->alloc_site;
  Pool *ptr_pool = static_cast<Pool *>(ptr_ctrl->pool);
  reinterpret_cast<T *>(ptr_blk->obj)->~T();
  ptr_pool->deallocate(ptr_blk);
  pool_trace_on_free(id, ptr_site);
}

template <class T>
class PoolPtr {
public:
  PoolPtr() noexcept = default;
  PoolPtr(std::nullptr_t) noexcept {}

  // adopts a block whose refcount is already 1
  PoolPtr(PoolCtrl *ptr_ctrl, T *ptr_obj) noexcept : m_ptr_ctrl(ptr_ctrl), m_ptr_obj(ptr_obj) {}

  // shares an existing control block and retains
  struct AliasTag {};
  PoolPtr(AliasTag, PoolCtrl *ptr_ctrl, T *ptr_obj) noexcept : m_ptr_ctrl(ptr_ctrl), m_ptr_obj(ptr_obj) { retain(); }

  PoolCtrl *ctrl_() const noexcept { return m_ptr_ctrl; }

  ~PoolPtr() { release(); }

  PoolPtr(const PoolPtr &ref_o) noexcept : m_ptr_ctrl(ref_o.m_ptr_ctrl), m_ptr_obj(ref_o.m_ptr_obj) { retain(); }
  PoolPtr &operator=(const PoolPtr &ref_o) noexcept {
    if (this != &ref_o) {
      release();
      m_ptr_ctrl = ref_o.m_ptr_ctrl;
      m_ptr_obj = ref_o.m_ptr_obj;
      retain();
    }
    return *this;
  }

  PoolPtr(PoolPtr &&ref_o) noexcept : m_ptr_ctrl(ref_o.m_ptr_ctrl), m_ptr_obj(ref_o.m_ptr_obj) {
    ref_o.m_ptr_ctrl = nullptr;
    ref_o.m_ptr_obj = nullptr;
  }
  PoolPtr &operator=(PoolPtr &&ref_o) noexcept {
    if (this != &ref_o) {
      release();
      m_ptr_ctrl = ref_o.m_ptr_ctrl;
      m_ptr_obj = ref_o.m_ptr_obj;
      ref_o.m_ptr_ctrl = nullptr;
      ref_o.m_ptr_obj = nullptr;
    }
    return *this;
  }

  template <class U, class = std::enable_if_t<std::is_convertible<U *, T *>::value>>
  PoolPtr(const PoolPtr<U> &ref_o) noexcept : m_ptr_ctrl(ref_o.m_ptr_ctrl), m_ptr_obj(ref_o.m_ptr_obj) { retain(); }
  template <class U, class = std::enable_if_t<std::is_convertible<U *, T *>::value>>
  PoolPtr(PoolPtr<U> &&ref_o) noexcept : m_ptr_ctrl(ref_o.m_ptr_ctrl), m_ptr_obj(ref_o.m_ptr_obj) {
    ref_o.m_ptr_ctrl = nullptr;
    ref_o.m_ptr_obj = nullptr;
  }

  T *get() const noexcept { return m_ptr_obj; }
  T *operator->() const noexcept { return m_ptr_obj; }
  T &operator*() const noexcept { return *m_ptr_obj; }
  explicit operator bool() const noexcept { return m_ptr_obj != nullptr; }

  void reset() noexcept {
    release();
    m_ptr_ctrl = nullptr;
    m_ptr_obj = nullptr;
  }

  uint32_t use_count() const noexcept {
    return m_ptr_ctrl ? m_ptr_ctrl->strong.load(std::memory_order_relaxed) : 0;
  }
  uint16_t pool_id() const noexcept { return m_ptr_ctrl ? m_ptr_ctrl->pool_id : 0; }
  const char *alloc_site() const noexcept { return m_ptr_ctrl ? m_ptr_ctrl->alloc_site : nullptr; }

  bool operator==(const PoolPtr &ref_o) const noexcept { return m_ptr_obj == ref_o.m_ptr_obj; }
  bool operator!=(const PoolPtr &ref_o) const noexcept { return m_ptr_obj != ref_o.m_ptr_obj; }
  bool operator==(std::nullptr_t) const noexcept { return m_ptr_obj == nullptr; }
  bool operator!=(std::nullptr_t) const noexcept { return m_ptr_obj != nullptr; }

private:
  void retain() noexcept {
    if (m_ptr_ctrl) m_ptr_ctrl->strong.fetch_add(1, std::memory_order_relaxed);
  }
  void release() noexcept {
    if (m_ptr_ctrl && m_ptr_ctrl->strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      m_ptr_ctrl->destroy_and_free(m_ptr_ctrl);
    }
  }

  PoolCtrl *m_ptr_ctrl = nullptr;
  T *m_ptr_obj = nullptr;

  template <class U>
  friend class PoolPtr;
};

// pool-backed dynamic cast, empty PoolPtr on failure
template <class D, class B>
PoolPtr<D> pool_dynamic_pointer_cast(const PoolPtr<B> &ref_p) noexcept {
  D *ptr_d = dynamic_cast<D *>(ref_p.get());
  if (ptr_d == nullptr) return PoolPtr<D>();
  return PoolPtr<D>(typename PoolPtr<D>::AliasTag{}, ref_p.ctrl_(), ptr_d);
}

// pool-backed static cast, caller must check the runtime tag first
template <class D, class B>
PoolPtr<D> pool_static_pointer_cast(const PoolPtr<B> &ref_p) noexcept {
  if (!ref_p) return PoolPtr<D>();
  return PoolPtr<D>(typename PoolPtr<D>::AliasTag{}, ref_p.ctrl_(), static_cast<D *>(ref_p.get()));
}

// prefer POOL_MAKE so the alloc site is the caller's location
template <class T, class Pool, class... Args>
PoolPtr<T> pool_ptr_make(Pool &ref_pool, uint16_t pool_id, const char *ptr_alloc_site, Args &&...args) {
  using Block = PoolBlock<T>;
  void *ptr_mem = ref_pool.allocate(sizeof(Block));
  Block *ptr_blk = static_cast<Block *>(ptr_mem);

  ::new (&ptr_blk->ctrl) PoolCtrl;
  ptr_blk->ctrl.strong.store(1, std::memory_order_relaxed);
  ptr_blk->ctrl.pool_id = pool_id;
  ptr_blk->ctrl.alloc_site = ptr_alloc_site;
  ptr_blk->ctrl.pool = &ref_pool;
  ptr_blk->ctrl.destroy_and_free = &pool_block_destroy<T, Pool>;

  T *ptr_obj = ::new (&ptr_blk->obj) T(std::forward<Args>(args)...);
  pool_trace_on_alloc(pool_id, ptr_alloc_site);
  return PoolPtr<T>(&ptr_blk->ctrl, ptr_obj);
}

} // namespace someIp

// compile-time file and line string for the alloc site
#define SOMEIP_POOLPTR_STR2(x) #x
#define SOMEIP_POOLPTR_STR(x) SOMEIP_POOLPTR_STR2(x)
#define SOMEIP_POOLPTR_SITE __FILE__ ":" SOMEIP_POOLPTR_STR(__LINE__)

// tags the allocation with the call site
#define POOL_MAKE(pool, pool_id, T, ...) \
  ::someIp::pool_ptr_make<T>((pool), (pool_id), SOMEIP_POOLPTR_SITE, ##__VA_ARGS__)

#endif // SOMEIP_UTILS_POOLPTR_HPP
