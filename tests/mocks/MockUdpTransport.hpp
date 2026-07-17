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

#ifndef SOMEIP_TESTS_MOCKUDPTRANSPORT_HPP
#define SOMEIP_TESTS_MOCKUDPTRANSPORT_HPP

#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>

#include "someIp/communication/ITransport.hpp"
#include "someIp/communication/ByteBuffer.hpp"
#include "someIp/communication/Endpoint.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "mocks/MockNetworkRouter.hpp"
#include "mocks/MockByteBuffer.hpp"

namespace someIp {
namespace test {

// ITransport backed by the in-process MockNetworkRouter (no sockets)
class MockUdpTransport : public ITransport {
public:
  MockUdpTransport() { MockNetworkRouter::instance().register_transport(this); }
  ~MockUdpTransport() override { MockNetworkRouter::instance().unregister_transport(this); }

  void init(const IpEndpoint &local) override {
    std::lock_guard<std::mutex> lk(mtx_);
    local_ = local;
  }
  void set_rx_callback(RxCallback cb) override {
    std::lock_guard<std::mutex> lk(mtx_);
    rx_ = std::move(cb);
  }
  bool open_listen_port(uint16_t port) override {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!contains(ports_, port)) ports_.push_back(port);
    return true;
  }
  bool join_multicast_group(const IpEndpoint &group) override {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!contains(groups_, group.v4)) groups_.push_back(group.v4);
    return true;
  }
  bool send_to(const IpEndpoint &dest, BufferPtr buf, uint16_t src_port = 0) override {
    if (!buf) {
      return false;
    }
    // snapshot outgoing bytes into a stack buffer then route
    uint8_t tmp[MAX_MOCK_DATAGRAM];
    size_t n = buf.size();
    if (n > MAX_MOCK_DATAGRAM) n = MAX_MOCK_DATAGRAM;
    if (const uint8_t *d = buf.data()) {
      std::memcpy(tmp, d, n);
    } else {
      n = 0;
    }
    MockNetworkRouter::instance().route(this, dest, tmp, n, src_port);
    return true;
  }

  // datagram transport, no stream operations
  ConnectionId connect(const IpEndpoint &) override { return {}; }
  bool is_connected(const IpEndpoint &) override { return false; }
  ConnectionId find_connection(const IpEndpoint &) override { return {}; }
  bool send_on(ConnectionId, BufferPtr) override { return false; }

  TransportKind kind() const override { return TransportKind::UDP; }

  uint32_t local_ip() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return local_.v4;
  }
  bool bound(uint16_t port) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return contains(ports_, port);
  }
  bool joined_group(uint32_t v4) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return contains(groups_, v4);
  }
  // deliver a datagram to this transport's RX sink, copying into a pooled buffer
  void deliver(const IpEndpoint &src, const uint8_t *data, size_t len) {
    RxCallback cb;
    {
      std::lock_guard<std::mutex> lk(mtx_);
      cb = rx_;
    }
    if (cb) {
      RxPacket pkt;
      pkt.source = src;
      pkt.buffer = make_pooled_buffer(data, len);
      cb(std::move(pkt));
    }
  }

private:
  template <class Vec, class T>
  static bool contains(const Vec &v, const T &x) {
    for (const auto &e : v)
      if (e == x) return true;
    return false;
  }

  mutable std::mutex mtx_;
  IpEndpoint local_;
  RxCallback rx_;
  StaticVector<uint16_t, 16> ports_;
  StaticVector<uint32_t, 8> groups_;
};

} // namespace test
} // namespace someIp

#endif // SOMEIP_TESTS_MOCKUDPTRANSPORT_HPP
