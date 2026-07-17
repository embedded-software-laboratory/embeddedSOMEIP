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

#ifndef SOMEIP_TESTS_MOCKNETWORKROUTER_HPP
#define SOMEIP_TESTS_MOCKNETWORKROUTER_HPP

#include <cstdint>
#include <cstddef>
#include <functional>
#include <mutex>

#include "someIp/communication/Endpoint.hpp"
#include "someIp/utils/StaticVector.hpp"

namespace someIp {
namespace test {

class MockUdpTransport;

// in-process virtual UDP network for tests
class MockNetworkRouter {
public:
  static MockNetworkRouter &instance();

  void register_transport(MockUdpTransport *t);
  void unregister_transport(MockUdpTransport *t);

  // deliver len bytes from sender to whichever transport(s) match dest
  void route(MockUdpTransport *sender, const IpEndpoint &dest,
             const uint8_t *data, size_t len, uint16_t src_port);

  using DropFilter = std::function<bool(MockUdpTransport *sender,
                                        MockUdpTransport *dst,
                                        const IpEndpoint &dest)>;
  void set_drop_filter(DropFilter f);
  void set_drop_rate(double rate); // 0.0..1.0 deterministic RNG
  void clear_drop_policy();

  // clear all registrations and policy
  void reset();

private:
  MockNetworkRouter() = default;

  bool should_drop(MockUdpTransport *sender, MockUdpTransport *dst,
                   const IpEndpoint &dest);

  mutable std::recursive_mutex mtx_;
  StaticVector<MockUdpTransport *, 16> transports_;
  DropFilter drop_filter_;
  double drop_rate_ = 0.0;
  uint64_t rng_state_ = 0x9E3779B97F4A7C15ull; // deterministic seed
};

} // namespace test
} // namespace someIp

#endif // SOMEIP_TESTS_MOCKNETWORKROUTER_HPP
