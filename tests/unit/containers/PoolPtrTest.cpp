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

#include <thread>
#include <vector>

#include "someIp/utils/PoolPtr.hpp"
#include "someIp/utils/PoolTrace.hpp"
#include "someIp/utils/PoolAllocator.hpp"

using namespace someIp;

namespace {

// pool id reserved for these tests (< pool_id::COUNT, distinct from real pools)
constexpr uint16_t TEST_POOL = pool_id::COUNT - 1;

struct Tracked {
  static int live;
  int v;
  explicit Tracked(int x = 0) : v(x) { ++live; }
  virtual ~Tracked() { --live; }
};
int Tracked::live = 0;

struct DerivedTracked : Tracked {
  static int derived_live;
  int w;
  DerivedTracked(int a, int b) : Tracked(a), w(b) { ++derived_live; }
  ~DerivedTracked() override { --derived_live; }
};
int DerivedTracked::derived_live = 0;

using TestPool = FixedBlockPool<256, 4>;

} // namespace

TEST(PoolPtrTest, BasicLifecycleDestroysAndFrees) {
  TestPool pool;
  Tracked::live = 0;
  {
    auto p = POOL_MAKE(pool, TEST_POOL, Tracked, 42);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->v, 42);
    EXPECT_EQ(p.use_count(), 1u);
    EXPECT_EQ(Tracked::live, 1);
  }
  EXPECT_EQ(Tracked::live, 0); // destroyed at scope exit
}

TEST(PoolPtrTest, CopyRetainsMoveSteals) {
  TestPool pool;
  Tracked::live = 0;
  auto p = POOL_MAKE(pool, TEST_POOL, Tracked, 1);
  {
    auto q = p; // copy = retain
    EXPECT_EQ(p.use_count(), 2u);
    EXPECT_EQ(Tracked::live, 1);
    auto r = std::move(q); // move = steal
    EXPECT_FALSE(q);
    EXPECT_TRUE(r);
    EXPECT_EQ(p.use_count(), 2u);
  }
  EXPECT_EQ(p.use_count(), 1u);
  EXPECT_EQ(Tracked::live, 1);
  p.reset();
  EXPECT_EQ(Tracked::live, 0);
}

TEST(PoolPtrTest, UpcastDestroysMostDerived) {
  TestPool pool;
  Tracked::live = 0;
  DerivedTracked::derived_live = 0;
  {
    PoolPtr<Tracked> base = POOL_MAKE(pool, TEST_POOL, DerivedTracked, 7, 9);
    EXPECT_EQ(base->v, 7);
    EXPECT_EQ(Tracked::live, 1);
    EXPECT_EQ(DerivedTracked::derived_live, 1);
  }
  // deleter was bound to DerivedTracked, so ~DerivedTracked ran
  EXPECT_EQ(DerivedTracked::derived_live, 0);
  EXPECT_EQ(Tracked::live, 0);
}

TEST(PoolPtrTest, ProvenanceRecorded) {
  TestPool pool;
  auto p = POOL_MAKE(pool, TEST_POOL, Tracked, 0);
  EXPECT_EQ(p.pool_id(), TEST_POOL);
  ASSERT_NE(p.alloc_site(), nullptr);
  EXPECT_NE(std::string(p.alloc_site()).find("PoolPtrTest"), std::string::npos);
}

TEST(PoolPtrTest, BlockReturnedToPoolForReuse) {
  TestPool pool; // capacity 4
  for (int round = 0; round < 3; ++round) {
    std::vector<PoolPtr<Tracked>> ptrs;
    for (int i = 0; i < 4; ++i) ptrs.push_back(POOL_MAKE(pool, TEST_POOL, Tracked, i));
    EXPECT_EQ(Tracked::live, 4);
    ptrs.clear(); // all released -> blocks returned
    EXPECT_EQ(Tracked::live, 0);
  }
}

TEST(PoolPtrTest, LiveCountTracksAllocations) {
  TestPool pool;
  const uint32_t base = pool_trace_live(TEST_POOL);
  {
    auto a = POOL_MAKE(pool, TEST_POOL, Tracked, 0);
    auto b = POOL_MAKE(pool, TEST_POOL, Tracked, 0);
    EXPECT_EQ(pool_trace_live(TEST_POOL), base + 2);
  }
  EXPECT_EQ(pool_trace_live(TEST_POOL), base); // returns to baseline, no leak
}

TEST(PoolPtrTest, RefcountThreadSafe) {
  TestPool pool;
  Tracked::live = 0;
  auto p = POOL_MAKE(pool, TEST_POOL, Tracked, 0);
  constexpr int kThreads = 8;
  constexpr int kIters = 2000;
  std::vector<std::thread> ts;
  for (int t = 0; t < kThreads; ++t) {
    ts.emplace_back([p]() mutable {
      for (int i = 0; i < kIters; ++i) {
        auto copy = p; // retain
        (void)copy->v; // release at loop end
      }
    });
  }
  for (auto &t : ts) t.join();
  EXPECT_EQ(p.use_count(), 1u); // all transient copies released
  EXPECT_EQ(Tracked::live, 1);
  p.reset();
  EXPECT_EQ(Tracked::live, 0);
}
