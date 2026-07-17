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

#include "someIp/utils/StaticMap.hpp"
#include "someIp/utils/StaticVector.hpp"

using someIp::StaticMap;
using someIp::StaticVector;

TEST(StaticMapTest, InsertFindErase) {
  StaticMap<int, int, 4> m;
  m[1] = 10;
  m[2] = 20;
  EXPECT_EQ(m.size(), 2u);
  EXPECT_EQ(m[1], 10);
  ASSERT_NE(m.find(2), m.end());
  EXPECT_EQ(m.find(2)->second, 20);
  EXPECT_EQ(m.find(3), m.end());
  EXPECT_EQ(m.erase(1), 1u);
  EXPECT_EQ(m.erase(1), 0u);
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m[2], 20);
}

TEST(StaticMapTest, OperatorIndexUpdatesExisting) {
  StaticMap<int, int, 4> m;
  m[5] = 1;
  m[5] = 2;
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m[5], 2);
}

TEST(StaticMapTest, FullDropsWriteWithoutGrowing) {
  StaticMap<int, int, 2> m;
  m[1] = 1;
  m[2] = 2;
  EXPECT_TRUE(m.full());
#ifdef NDEBUG
  // release build writes to a scratch slot instead of growing
  m[3] = 3;
  EXPECT_EQ(m.size(), 2u);
  EXPECT_EQ(m.find(3), m.end());
#else
  // debug build asserts instead
  ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_DEATH({ m[3] = 3; }, "capacity");
#endif
}

TEST(StaticMapTest, EraseShiftHelperForVector) {
  StaticVector<int, 8> v;
  for (int i = 0; i < 5; ++i) v.push_back(i);
  v.erase(v.begin() + 1, v.begin() + 3);
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 0);
  EXPECT_EQ(v[1], 3);
  EXPECT_EQ(v[2], 4);
  v.erase(v.begin());
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], 3);
}
