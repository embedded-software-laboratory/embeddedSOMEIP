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

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "someIp/structs/PacketBuffer.hpp"

using someIp::PacketBuffer;

namespace {

PacketBuffer from_string(const std::string &s) {
  return PacketBuffer(s.data(), s.size());
}

std::string to_string(const PacketBuffer &b) {
  return std::string(reinterpret_cast<const char *>(b.data()), b.size());
}

} // namespace

TEST(PacketBuffer, DefaultIsEmpty) {
  PacketBuffer b;
  EXPECT_TRUE(b.empty());
  EXPECT_FALSE(static_cast<bool>(b));
  EXPECT_EQ(b.size(), 0u);
}

TEST(PacketBuffer, SizedConstructorIsNotEmpty) {
  PacketBuffer b(8);
  EXPECT_FALSE(b.empty());
  EXPECT_TRUE(static_cast<bool>(b));
  EXPECT_EQ(b.size(), 8u);
}

TEST(PacketBuffer, CopiesFromSource) {
  PacketBuffer b = from_string("hello");
  EXPECT_EQ(b.size(), 5u);
  EXPECT_EQ(to_string(b), "hello");
}

TEST(PacketBuffer, CopyOutRespectsOffset) {
  PacketBuffer b = from_string("abcdef");
  char out[3] = {};
  EXPECT_EQ(b.copy_out(out, 3, 2), 3u);
  EXPECT_EQ(std::string(out, 3), "cde");
}

TEST(PacketBuffer, CopyOutRejectsOutOfBounds) {
  PacketBuffer b = from_string("abc");
  char out[8] = {};
  EXPECT_EQ(b.copy_out(out, 4, 0), 0u);  // longer than the buffer
  EXPECT_EQ(b.copy_out(out, 2, 2), 0u);  // offset + n runs past the end
  EXPECT_EQ(b.copy_out(out, 0, 0), 0u);  // zero length is rejected
}

TEST(PacketBuffer, RemoveFrontAdvancesCursor) {
  PacketBuffer b = from_string("HEADERBODY");
  EXPECT_TRUE(b.remove_front(6));
  EXPECT_EQ(b.size(), 4u);
  EXPECT_EQ(to_string(b), "BODY");
}

TEST(PacketBuffer, RemoveFrontRejectsOverlongN) {
  PacketBuffer b = from_string("abc");
  EXPECT_FALSE(b.remove_front(4));
  EXPECT_EQ(b.size(), 3u) << "a rejected remove_front must not consume anything";
}

TEST(PacketBuffer, RemoveFrontOfEverythingLeavesItEmpty) {
  PacketBuffer b = from_string("abc");
  EXPECT_TRUE(b.remove_front(3));
  EXPECT_TRUE(b.empty());
  EXPECT_EQ(b.size(), 0u);
}

TEST(PacketBuffer, AddInFrontPrepends) {
  PacketBuffer b = from_string("BODY");
  const char hdr[] = {'H', 'D', 'R'};
  b.add_in_front(hdr, sizeof(hdr));
  EXPECT_EQ(b.size(), 7u);
  EXPECT_EQ(to_string(b), "HDRBODY");
}

// the front cursor lets a strip-then-prepend avoid recopying the tail
TEST(PacketBuffer, AddInFrontReusesTrimmedFrontSpace) {
  PacketBuffer b = from_string("HEADERBODY");
  ASSERT_TRUE(b.remove_front(6));
  const uint8_t *tail_before = b.data();

  const char hdr[] = {'X', 'Y'};
  b.add_in_front(hdr, sizeof(hdr));

  EXPECT_EQ(to_string(b), "XYBODY");
  EXPECT_EQ(b.data(), tail_before - 2) << "prepend that fits the trimmed front should not reallocate";
}

TEST(PacketBuffer, AddInFrontGrowsWhenFrontSpaceIsTooSmall) {
  PacketBuffer b = from_string("BODY");
  const char hdr[] = {'1', '2', '3', '4', '5', '6'};
  b.add_in_front(hdr, sizeof(hdr));
  EXPECT_EQ(to_string(b), "123456BODY");
}

TEST(PacketBuffer, AppendExtends) {
  PacketBuffer b = from_string("AB");
  const char more[] = {'C', 'D'};
  b.append(more, sizeof(more));
  EXPECT_EQ(to_string(b), "ABCD");
}

TEST(PacketBuffer, SliceCopyLeavesSourceIntact) {
  PacketBuffer b = from_string("abcdef");
  PacketBuffer head = b.slice_copy(3, 1);
  EXPECT_EQ(to_string(head), "bcd");
  EXPECT_EQ(to_string(b), "abcdef") << "slice_copy must not consume the source";
}

TEST(PacketBuffer, SliceCopyReturnsEmptyOnOutOfBounds) {
  PacketBuffer b = from_string("abc");
  EXPECT_TRUE(b.slice_copy(4, 0).empty());
  EXPECT_TRUE(b.slice_copy(2, 2).empty());
  EXPECT_TRUE(b.slice_copy(0, 0).empty());
  EXPECT_EQ(to_string(b), "abc") << "a rejected slice_copy must not disturb the source";
}

TEST(PacketBuffer, SliceMoveSplitsAndKeepsRemainder) {
  PacketBuffer b = from_string("HEADERBODY");
  PacketBuffer head = b.slice_move(6);
  EXPECT_EQ(to_string(head), "HEADER");
  EXPECT_EQ(to_string(b), "BODY") << "slice_move must leave the remainder behind";
}

TEST(PacketBuffer, SliceMoveOfWholeBufferDrainsSource) {
  PacketBuffer b = from_string("abc");
  PacketBuffer head = b.slice_move(3);
  EXPECT_EQ(to_string(head), "abc");
  // slice_move only advances the front cursor, so it still owns its memory
  EXPECT_TRUE(b.empty());
  EXPECT_EQ(b.size(), 0u);
}

TEST(PacketBuffer, SliceMoveReturnsEmptyOnOutOfBounds) {
  PacketBuffer b = from_string("abc");
  EXPECT_TRUE(b.slice_move(4).empty());
  EXPECT_TRUE(b.slice_move(0).empty());
  EXPECT_EQ(to_string(b), "abc") << "a rejected slice_move must not consume the source";
}

TEST(PacketBuffer, TakeBytesDropsTrimmedFront) {
  PacketBuffer b = from_string("HEADERBODY");
  ASSERT_TRUE(b.remove_front(6));
  std::vector<uint8_t> bytes = b.take_bytes();
  ASSERT_EQ(bytes.size(), 4u);
  EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "BODY");
}

TEST(PacketBuffer, MoveConstructorTransfersOwnership) {
  PacketBuffer a = from_string("payload");
  PacketBuffer b(std::move(a));
  EXPECT_EQ(to_string(b), "payload");
  EXPECT_TRUE(a.empty()); // NOLINT(bugprone-use-after-move) -- asserting the moved-from state
}

TEST(PacketBuffer, MoveAssignmentTransfersOwnership) {
  PacketBuffer a = from_string("payload");
  PacketBuffer b = from_string("discarded");
  b = std::move(a);
  EXPECT_EQ(to_string(b), "payload");
  EXPECT_TRUE(a.empty()); // NOLINT(bugprone-use-after-move) -- asserting the moved-from state
}

// mirrors the TpPackage serialize path
TEST(PacketBuffer, HeaderPrependRoundTrip) {
  PacketBuffer b = from_string("PAYLOAD");
  const uint8_t hdr[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  b.add_in_front(hdr, sizeof(hdr));

  ASSERT_EQ(b.size(), 4u + 7u);
  EXPECT_EQ(std::memcmp(b.data(), hdr, 4), 0);
  EXPECT_EQ(std::string(reinterpret_cast<const char *>(b.data()) + 4, 7), "PAYLOAD");

  ASSERT_TRUE(b.remove_front(4));
  EXPECT_EQ(to_string(b), "PAYLOAD");
}
