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

#ifndef SOMEIP_TESTS_MOCKHARNESS_HPP
#define SOMEIP_TESTS_MOCKHARNESS_HPP

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "someIp/ESomeIp.hpp"
#include "someIp/net/IpAddress.hpp"
#include "someIp/communication/Endpoint.hpp"
#include "someIp/pattern/Singleton.hpp"
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/transport-protocol/Reassembler.hpp"

#include "mocks/MockByteBuffer.hpp"
#include "mocks/MockUdpTransport.hpp"
#include "mocks/MockNetworkRouter.hpp"

namespace someIp {
namespace test {

inline IpAddr make_ip(const char *s) { return IpAddr::from_string(s); }

inline IpEndpoint make_endpoint(const char *s, uint16_t port) {
  return IpEndpoint{make_ip(s).v4, port};
}

// PacketBuffer is contiguous, so the old pbuf chain walk is one copy
inline std::string flatten(PackageRx &p) {
  return std::string(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
}

inline bool wait_for(std::atomic<bool> &flag, int timeout_ms = 2000) {
  for (int i = 0; i < timeout_ms / 5 && !flag.load(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return flag.load();
}

inline bool wait_until(const std::function<bool()> &pred, int timeout_ms = 3000) {
  for (int i = 0; i < timeout_ms / 5 && !pred(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return pred();
}

// registers a single in-process mock UDP transport
inline ESomeIp::TransportPopulator mock_populator() {
  return [](TransportRegistry &reg) {
    reg.emplace_udp<MockUdpTransport>();
  };
}

// base fixture for in-process mock-network tests
class MockHarnessTest : public ::testing::Test {
protected:
  void SetUp() override {
    MockNetworkRouter::instance().reset();
    // reset singletons so cross-test state cannot leak
    Singleton<ServiceHandler>::reset_instance();
    Singleton<tp::Reassembler>::reset_instance();
  }

  void TearDown() override {
    EXPECT_EQ(mock_pool_live_blocks().load(), 0) << "test leaked mock datagram pool blocks";
  }
};

} // namespace test
} // namespace someIp

#endif // SOMEIP_TESTS_MOCKHARNESS_HPP
