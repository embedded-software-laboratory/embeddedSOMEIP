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

#include <atomic>
#include <string>

#include "someIp/ThreadPool.hpp"
#include "someIp/communication/TransportRegistry.hpp"
#include "someIp/handler/RxContext.hpp"
#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/PackageRx.hpp"

#include "mocks/MockNetworkRouter.hpp"
#include "mocks/MockUdpTransport.hpp"
#include "utils/MockHarness.hpp"

using namespace someIp;

namespace {

// the fixture owns the registry and context for the pool's lifetime
class ThreadPoolTests : public test::MockHarnessTest {
protected:
  void SetUp() override {
    test::MockHarnessTest::SetUp();
    m_transports.emplace_udp<test::MockUdpTransport>();
  }

  TransportRegistry m_transports;
  RxContext m_rxContext;
};

} // namespace

TEST_F(ThreadPoolTests, StartAndStopThreads) {
  ThreadPool pool(m_transports, &m_rxContext);

  EXPECT_TRUE(pool.start_threads());
  EXPECT_TRUE(pool.stop_threads());
}

TEST_F(ThreadPoolTests, StopWithoutStartIsSafe) {
  ThreadPool pool(m_transports, &m_rxContext);

  // stopping a pool that never started must not hang or crash
  pool.stop_threads();
}

TEST_F(ThreadPoolTests, AddBufferToQueueAcceptsPackage) {
  ThreadPool pool(m_transports, &m_rxContext);
  ASSERT_TRUE(pool.start_threads());

  Package package(static_cast<uint16_t>(0x1234), static_cast<uint16_t>(1), "PING",
                  test::make_ip("192.168.1.10"), 8000);
  package.header._message_type = MessageType::REQUEST_NO_RETURN;

  PackageRx rx(package.serialize(), test::make_ip("192.168.1.20"), 8000);
  EXPECT_TRUE(pool.add_buffer_to_queue(std::move(rx)));

  EXPECT_TRUE(pool.stop_threads());
}
