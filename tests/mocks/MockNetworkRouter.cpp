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

#include "mocks/MockNetworkRouter.hpp"
#include "mocks/MockUdpTransport.hpp"

#include <algorithm>

namespace someIp {
namespace test {

MockNetworkRouter &MockNetworkRouter::instance() {
  static MockNetworkRouter router;
  return router;
}

void MockNetworkRouter::register_transport(MockUdpTransport *t) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  transports_.push_back(t);
}

void MockNetworkRouter::unregister_transport(MockUdpTransport *t) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  transports_.erase(std::remove(transports_.begin(), transports_.end(), t), transports_.end());
}

void MockNetworkRouter::set_drop_filter(DropFilter f) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  drop_filter_ = std::move(f);
}

void MockNetworkRouter::set_drop_rate(double rate) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  drop_rate_ = rate;
}

void MockNetworkRouter::clear_drop_policy() {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  drop_filter_ = nullptr;
  drop_rate_ = 0.0;
}

void MockNetworkRouter::reset() {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  transports_.clear();
  drop_filter_ = nullptr;
  drop_rate_ = 0.0;
  rng_state_ = 0x9E3779B97F4A7C15ull;
}

bool MockNetworkRouter::should_drop(MockUdpTransport *sender, MockUdpTransport *dst,
                                    const IpEndpoint &dest) {
  if (drop_filter_ && drop_filter_(sender, dst, dest)) {
    return true;
  }
  if (drop_rate_ > 0.0) {
    // deterministic xorshift so tests are reproducible
    rng_state_ ^= rng_state_ << 13;
    rng_state_ ^= rng_state_ >> 7;
    rng_state_ ^= rng_state_ << 17;
    double r = static_cast<double>(rng_state_ % 1000000ull) / 1000000.0;
    if (r < drop_rate_) {
      return true;
    }
  }
  return false;
}

void MockNetworkRouter::route(MockUdpTransport *sender, const IpEndpoint &dest,
                              const uint8_t *data, size_t len, uint16_t src_port) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);

  const bool multicast = (dest.v4 >> 28) == 0xE;
  const IpEndpoint src{sender ? sender->local_ip() : 0u, src_port};

  for (MockUdpTransport *t : transports_) {
    if (t == sender) {
      continue; // no self-delivery
    }
    const bool match = multicast ? (t->joined_group(dest.v4) && t->bound(dest.port))
                                  : (t->local_ip() == dest.v4 && t->bound(dest.port));
    if (!match || should_drop(sender, t, dest)) {
      continue;
    }
    t->deliver(src, data, len);
  }
}

} // namespace test
} // namespace someIp
