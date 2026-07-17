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

#ifndef SOMEIP_UTILS_POOLALLOCATOR_HPP
#define SOMEIP_UTILS_POOLALLOCATOR_HPP

#include <cstddef>
#include <new>
#include <type_traits>

#ifdef SOMEIP_PLATFORM_STM32
#include "lwip/debug.h"
#else
#include <cassert>
// host has no lwIP, map LWIP_ASSERT onto assert
#ifndef LWIP_ASSERT
#define LWIP_ASSERT(msg, cond) assert((cond) && (msg))
#endif
#endif
#include "someIp/os/Os.hpp"
#include "someIp/utils/CapacityCheck.hpp"

namespace someIp {

// thread-safe fixed-block pool, no heap fallback (M-1)
template <size_t BlockSize, size_t NumBlocks>
class FixedBlockPool {
public:
  void *allocate(size_t bytes) {
    LWIP_ASSERT("FixedBlockPool: request exceeds block size", bytes <= BlockSize);
    (void)bytes; // referenced only by the debug assert
    os::Guard g(m_mtx);
    for (size_t i = 0; i < NumBlocks; ++i) {
      if (!m_used[i]) {
        m_used[i] = true;
        return &m_blocks[i];
      }
    }
    capacity_exhausted("FixedBlockPool (no heap fallback)");
    return nullptr; // soft policy, callers treat null as failure
  }

  void deallocate(void *ptr_p) {
    if (ptr_p == nullptr) return;
    LWIP_ASSERT("FixedBlockPool: foreign pointer in deallocate",
                ptr_p >= &m_blocks[0] && ptr_p <= &m_blocks[NumBlocks - 1]);
    os::Guard g(m_mtx);
    size_t i = static_cast<size_t>(static_cast<Block *>(ptr_p) - &m_blocks[0]);
    m_used[i] = false;
  }

private:
  struct alignas(alignof(std::max_align_t)) Block {
    unsigned char bytes[BlockSize];
  };
  Block m_blocks[NumBlocks];
  bool m_used[NumBlocks] = {};
  os::Mutex m_mtx;
};

// stl allocator drawing from a FixedBlockPool
template <class T, size_t BlockSize, size_t NumBlocks>
struct PoolAllocator {
  using value_type = T;
  using Pool = FixedBlockPool<BlockSize, NumBlocks>;

  // explicit rebind, the non-type params block the allocator_traits default
  // keep lowercase, allocator_traits looks these up by name
  template <class U>
  struct rebind {
    using other = PoolAllocator<U, BlockSize, NumBlocks>;
  };

  Pool *pool;

  explicit PoolAllocator(Pool *ptr_p) noexcept : pool(ptr_p) {}
  template <class U>
  PoolAllocator(const PoolAllocator<U, BlockSize, NumBlocks> &ref_o) noexcept : pool(ref_o.pool) {}

  T *allocate(size_t n) { return static_cast<T *>(pool->allocate(n * sizeof(T))); }
  void deallocate(T *ptr_p, size_t) noexcept { pool->deallocate(ptr_p); }

  template <class U>
  bool operator==(const PoolAllocator<U, BlockSize, NumBlocks> &ref_o) const noexcept { return pool == ref_o.pool; }
  template <class U>
  bool operator!=(const PoolAllocator<U, BlockSize, NumBlocks> &ref_o) const noexcept { return pool != ref_o.pool; }
};

} // namespace someIp

#endif // SOMEIP_UTILS_POOLALLOCATOR_HPP
