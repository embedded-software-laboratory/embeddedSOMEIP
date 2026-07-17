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

#include "someIp/utils/StaticVector.hpp"

using someIp::StaticVector;

TEST(StaticVectorTest, PushPopSizeIndex) {
  StaticVector<int, 4> v;
  EXPECT_TRUE(v.empty());
  EXPECT_EQ(v.capacity(), 4u);
  EXPECT_TRUE(v.push_back(10));
  EXPECT_TRUE(v.push_back(20));
  EXPECT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], 10);
  EXPECT_EQ(v.back(), 20);
  v.pop_back();
  EXPECT_EQ(v.size(), 1u);
  EXPECT_EQ(v.back(), 10);
}

TEST(StaticVectorTest, RejectsOverflowWithoutGrowing) {
  StaticVector<int, 2> v;
  EXPECT_TRUE(v.push_back(1));
  EXPECT_TRUE(v.push_back(2));
  EXPECT_TRUE(v.full());
#ifdef NDEBUG
  // release refuses the push without growing
  EXPECT_FALSE(v.push_back(3));
  EXPECT_EQ(v.size(), 2u);
#else
  // debug asserts on a mis-sized cap
  ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_DEATH({ v.push_back(3); }, "capacity");
#endif
}

TEST(StaticVectorTest, RangeForAndClear) {
  StaticVector<int, 8> v;
  for (int i = 1; i <= 5; ++i) v.push_back(i);
  int sum = 0;
  for (int x : v) sum += x;
  EXPECT_EQ(sum, 15);
  v.clear();
  EXPECT_TRUE(v.empty());
}

TEST(StaticVectorTest, HoldsSharedPtrWithoutLeaking) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> weak = sp;
  {
    StaticVector<std::shared_ptr<int>, 4> v;
    v.push_back(sp);
    v.push_back(std::move(sp));
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(*v[0], 42);
    v.clear();
  }
  EXPECT_EQ(weak.use_count(), 0);
}
