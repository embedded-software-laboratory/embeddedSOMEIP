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

#include <vector>

#include "someIp/communication/ByteBuffer.hpp"

using someIp::ByteBuffer;
using someIp::make_vector_buffer;

TEST(ByteBufferTest, EmptyByDefault) {
  ByteBuffer b;
  EXPECT_FALSE(static_cast<bool>(b));
  EXPECT_EQ(b.size(), 0u);
  EXPECT_EQ(b.data(), nullptr);
}

TEST(ByteBufferTest, VectorBackedExposesBytes) {
  std::vector<uint8_t> bytes{1, 2, 3, 4};
  ByteBuffer b = make_vector_buffer(bytes);
  ASSERT_TRUE(static_cast<bool>(b));
  ASSERT_EQ(b.size(), 4u);
  EXPECT_EQ(b.data()[0], 1);
  EXPECT_EQ(b.data()[3], 4);
}

TEST(ByteBufferTest, MoveTransfersOwnership) {
  ByteBuffer a = make_vector_buffer(std::vector<uint8_t>{9, 8, 7});
  ByteBuffer b = std::move(a);
  EXPECT_FALSE(static_cast<bool>(a));
  ASSERT_TRUE(static_cast<bool>(b));
  EXPECT_EQ(b.size(), 3u);
  EXPECT_EQ(b.data()[0], 9);
}

TEST(ByteBufferTest, VectorBackedHasNoNativeHandle) {
  ByteBuffer b = make_vector_buffer(std::vector<uint8_t>{1});
  EXPECT_EQ(b.release_native(), nullptr); // not pbuf-backed
}
