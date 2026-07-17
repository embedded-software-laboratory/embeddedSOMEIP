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

#include "someIp/utils/SessionTable.hpp"

using someIp::SessionTable;

namespace {
struct Slot {
  bool active = false;
  uint16_t id = 0;
  int payload = 0;

  bool matches(uint16_t k) const { return id == k; }
  void set_key(uint16_t k) { id = k; }
};
} // namespace

TEST(SessionTableTest, AcquireFindRelease) {
  SessionTable<Slot, 4> t;
  EXPECT_FALSE(t.has(7));
  Slot *s = t.acquire_free(7);
  ASSERT_NE(s, nullptr);
  s->payload = 42;
  EXPECT_TRUE(t.has(7));
  EXPECT_EQ(t.find(7)->payload, 42);
  EXPECT_EQ(t.active_count(), 1u);
  EXPECT_TRUE(t.release(7));
  EXPECT_FALSE(t.has(7));
  EXPECT_FALSE(t.release(7));
}

TEST(SessionTableTest, AcquireReturnsExisting) {
  SessionTable<Slot, 4> t;
  Slot *a = t.acquire(5);
  a->payload = 1;
  Slot *b = t.acquire(5); // same id -> same slot
  EXPECT_EQ(a, b);
  EXPECT_EQ(t.active_count(), 1u);
}

TEST(SessionTableTest, FullReturnsNull) {
  SessionTable<Slot, 2> t;
  ASSERT_NE(t.acquire_free(1), nullptr);
  ASSERT_NE(t.acquire_free(2), nullptr);
  EXPECT_EQ(t.acquire_free(3), nullptr); // no free slot
  EXPECT_EQ(t.active_count(), 2u);
}

TEST(SessionTableTest, ReleaseResetsSlot) {
  SessionTable<Slot, 2> t;
  Slot *s = t.acquire_free(9);
  s->payload = 99;
  t.release(9);
  Slot *reused = t.acquire_free(9);
  EXPECT_EQ(reused->payload, 0); // slot was reset on release
}
