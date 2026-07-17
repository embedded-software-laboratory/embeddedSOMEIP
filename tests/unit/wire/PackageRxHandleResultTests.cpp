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

#include "mocks/MockCallback.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/structs/PackageRxHandleResult.hpp"
#include "someIp/storages/ThreadSafeCircularBuffer.hpp"

#include "utils/SuppressOutput.hpp"

using namespace someIp;

class PackageRxHandleResultTests : public ::testing::Test {
protected:
  // PacketBuffer is move-only, so each test gets its own buffer
  PacketBuffer make_payload() const {
    PacketBuffer buf(PAYLOAD_SIZE);
    memset(buf.data(), 1, PAYLOAD_SIZE);
    return buf;
  }

  static constexpr size_t PAYLOAD_SIZE = 12;

  //SuppressOutput suppress; // Remove to see Debug output of SOME/IP
};

TEST_F(PackageRxHandleResultTests, CreateWithPackageRx) {
  IpAddr ip;
  auto rx = PackageRx(make_payload(), ip, 0);
  ASSERT_EQ(rx.payload.size(), PAYLOAD_SIZE);

  PackageRxHandleResult result;
  result.rx = std::move(rx);

  ASSERT_EQ(result.rx.payload.size(), PAYLOAD_SIZE);
}

TEST_F(PackageRxHandleResultTests, Move) {
  IpAddr ip;
  auto rx = PackageRx(make_payload(), ip, 0);

  PackageRxHandleResult result;
  result.rx = std::move(rx);

  PackageRxHandleResult movedResult;
  movedResult = std::move(result);

  ASSERT_EQ(movedResult.rx.payload.size(), PAYLOAD_SIZE);
}

TEST_F(PackageRxHandleResultTests, MoveWithPackageRx) {
  IpAddr ip;
  auto rx = PackageRx(make_payload(), ip, 0);

  PackageRxHandleResult result;
  result.rx = std::move(rx);

  PackageRxHandleResult movedResult;
  movedResult = std::move(result);

  ASSERT_EQ(movedResult.rx.payload.size(), PAYLOAD_SIZE);
}

TEST_F(PackageRxHandleResultTests, StaysUnchangedInBuffer) {
  ThreadSafeCircularBuffer<PackageRxHandleResult, 10> buf;
  buf.init();

  IpAddr ip;
  auto rx = PackageRx(make_payload(), ip, 0);
  PackageRxHandleResult result;
  result.rx = std::move(rx);

  buf.moveElementIntoBuffer(std::move(result));

  bool successful = false;
  PackageRxHandleResult element = buf.moveFirstInto(&successful);
  ASSERT_TRUE(successful);
  ASSERT_EQ(element.rx.payload.size(), PAYLOAD_SIZE);

  for (size_t i = 0; i < PAYLOAD_SIZE; i++) {
    ASSERT_EQ(element.rx.payload.data()[i], 1);
  }
}
