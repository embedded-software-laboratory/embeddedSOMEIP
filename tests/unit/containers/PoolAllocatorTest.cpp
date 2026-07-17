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

#include <gtest/gtest.h>

#include <memory>

#include "utils/AllocCounter.hpp"
#include "someIp/utils/PoolAllocator.hpp"

using someIp::FixedBlockPool;
using someIp::PoolAllocator;
namespace alloc = someIp::test::alloc;

namespace {
struct Small {
  int a, b, c, d; // 16 bytes
};
struct Big {
  char data[512]; // larger than the pool block
};
} // namespace

// allocate_shared via the pool must not touch the C++ heap
TEST(PoolAllocatorTest, AllocateSharedUsesPoolNoHeap) {
  FixedBlockPool<128, 8> pool;
  PoolAllocator<Small, 128, 8> a(&pool);

  alloc::arm();
  auto p = std::allocate_shared<Small>(a, Small{1, 2, 3, 4});
  alloc::disarm();

  EXPECT_NE(p, nullptr);
  EXPECT_EQ(p->a, 1);
  EXPECT_EQ(alloc::count(), 0u) << "pool-backed allocate_shared hit the heap";
}

// oversize request must assert rather than fall back to the heap
TEST(PoolAllocatorDeathTest, OversizeAsserts) {
  FixedBlockPool<128, 8> pool;
  PoolAllocator<Big, 128, 8> a(&pool);
  EXPECT_DEATH({ auto p = std::allocate_shared<Big>(a); (void)p; }, "");
}

// blocks are reused after the shared_ptr is released
TEST(PoolAllocatorTest, BlocksAreReused) {
  FixedBlockPool<128, 2> pool;
  PoolAllocator<Small, 128, 2> a(&pool);

  auto p1 = std::allocate_shared<Small>(a);
  void *raw1 = p1.get();
  p1.reset(); // returns the block

  auto p2 = std::allocate_shared<Small>(a);
  EXPECT_EQ(p2.get(), raw1) << "freed block should be reused";
}
