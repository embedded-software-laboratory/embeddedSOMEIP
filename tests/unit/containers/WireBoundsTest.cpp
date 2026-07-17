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

#include "someIp/utils/WireBounds.hpp"

using namespace someIp::wire;

TEST(WireBounds, WithinBoundAcceptsNormal) {
  EXPECT_TRUE(within_bound(0, 100, 20480));
  EXPECT_TRUE(within_bound(20000, 480, 20480));
  EXPECT_TRUE(within_bound(20480, 0, 20480));
}

TEST(WireBounds, WithinBoundRejectsOverMax) {
  EXPECT_FALSE(within_bound(20480, 1, 20480));
  EXPECT_FALSE(within_bound(0, 20481, 20480));
}

// C-1 offset+len wraps mod 2^32 but must be rejected in uint64_t
TEST(WireBounds, WithinBoundRejectsWraparound) {
  EXPECT_FALSE(within_bound(0xFFFFFFF0u, 0x20u, 20480));
  EXPECT_FALSE(within_bound(0xFFFFFFFFu, 1u, 20480));
}

TEST(WireBounds, SomeipLengthAcceptsNormal) {
  EXPECT_TRUE(valid_someip_length(13, 20480));
  EXPECT_TRUE(valid_someip_length(20472, 20480));
}

// C-2 the +8 must not wrap and must be rejected before pbuf_alloc truncates
TEST(WireBounds, SomeipLengthRejectsPlus8Wrap) {
  EXPECT_FALSE(valid_someip_length(0xFFFFFFF8u, 20480));
  EXPECT_FALSE(valid_someip_length(0xFFFFFFFFu, 20480));
}

TEST(WireBounds, SomeipLengthRejectsOverU16) {
  EXPECT_FALSE(valid_someip_length(0x10000u, 0x20000u));
  EXPECT_FALSE(valid_someip_length(0xFFFFu, 0x20000u));
}
