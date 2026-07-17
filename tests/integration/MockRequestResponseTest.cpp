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
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/pattern/Singleton.hpp"
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/config/config_linux.hpp"

#include "mocks/MockUdpTransport.hpp"
#include "mocks/MockNetworkRouter.hpp"
#include "utils/MockHarness.hpp"

using namespace someIp;
// make_ip / flatten / wait_for / mock_populator come from the shared harness
using namespace someIp::test;

// mock router alone must deliver a unicast datagram between transports
TEST(MockHarness, RouterDeliversUnicast) {
  test::MockNetworkRouter::instance().reset();

  test::MockUdpTransport a;
  test::MockUdpTransport b;
  a.init(IpEndpoint(0x0A0A0A01u, 0)); // 10.10.10.1
  b.init(IpEndpoint(0x0A0A0A02u, 0)); // 10.10.10.2

  std::atomic<bool> got{false};
  std::string payload;
  b.set_rx_callback([&](RxPacket &&p) {
    payload.assign(reinterpret_cast<const char *>(p.buffer.data()), p.buffer.size());
    got = true;
  });
  b.open_listen_port(8000);

  std::vector<uint8_t> bytes{'H', 'I'};
  a.send_to(IpEndpoint(0x0A0A0A02u, 8000), make_vector_buffer(bytes), 7000);

  EXPECT_TRUE(got.load());
  EXPECT_EQ(payload, "HI");
}

// synchronous fire_and_forget must reach a peer transport
TEST(MockHarness, ApiFireAndForgetReachesPeer) {
  test::MockNetworkRouter::instance().reset();
  Singleton<ServiceHandler>::reset_instance();

  ESomeIp a(mock_populator());
  a.init(make_ip("192.168.1.30"));
  a.set_transport_mode(TransportKind::UDP);

  test::MockUdpTransport peer;
  peer.init(IpEndpoint(0xC0A80128u, 0)); // 192.168.1.40
  std::atomic<bool> got{false};
  peer.set_rx_callback([&](RxPacket &&) { got = true; });
  peer.open_listen_port(9000);

  Package p(static_cast<uint16_t>(0x1), static_cast<uint16_t>(0x1), "X", make_ip("192.168.1.40"), 9000);
  p.header._message_type = MessageType::REQUEST_NO_RETURN;
  a.fire_and_forget(std::move(p), TransportKind::UDP);

  EXPECT_TRUE(got.load()) << "fire_and_forget did not reach the peer";
}

// request_and_response sender thread actually transmits
TEST(MockHarness, ApiRequestResponseSenderThread) {
  test::MockNetworkRouter::instance().reset();
  Singleton<ServiceHandler>::reset_instance();

  ESomeIp a(mock_populator());
  a.init(make_ip("192.168.1.50"));
  a.set_transport_mode(TransportKind::UDP);

  test::MockUdpTransport peer;
  peer.init(IpEndpoint(0xC0A80128u, 0)); // 192.168.1.40
  std::atomic<bool> got{false};
  peer.set_rx_callback([&](RxPacket &&) { got = true; });
  peer.open_listen_port(9000);

  Package p(static_cast<uint16_t>(0x1), static_cast<uint16_t>(0x1), "X", make_ip("192.168.1.40"), 9000);
  p.header._message_type = MessageType::REQUEST;
  a.request_and_response(std::move(p), [](PackageRx &&) {}, TransportKind::UDP);

  EXPECT_TRUE(wait_for(got)) << "sender thread did not transmit the queued request";
}

// client and server exchange a request/response over the mock network
TEST(MockHarness, RequestResponseTwoParticipants) {
  test::MockNetworkRouter::instance().reset();
  Singleton<ServiceHandler>::reset_instance();

  ESomeIp server(mock_populator());
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(8000);

  std::atomic<bool> got_request{false};
  Callback method_cb([&](PackageRx &&p) {
    got_request = true;
    server.send_response(std::move(p), "PONG");
  });
  Service svc(static_cast<uint16_t>(0x1234));
  svc.register_method(Method(static_cast<uint16_t>(1), std::move(method_cb)));
  server.register_service(std::move(svc));

  ESomeIp client(mock_populator());
  someIp::IpAddr client_ip = make_ip("192.168.1.20");
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  // UDP TX sends from SD_PORT so the response returns there
  client.listen_to_port(config::SD_PORT);

  std::atomic<bool> got_response{false};
  std::string response_payload;
  Package req(static_cast<uint16_t>(0x1234), static_cast<uint16_t>(1), "PING", server_ip, 8000);
  req.header._message_type = MessageType::REQUEST;
  client.request_and_response(
      std::move(req),
      [&](PackageRx &&p) {
        response_payload = flatten(p);
        got_response = true;
      },
      TransportKind::UDP);

  EXPECT_TRUE(wait_for(got_request)) << "server never received the request";
  EXPECT_TRUE(wait_for(got_response)) << "client never received the response";
  EXPECT_NE(response_payload.find("PONG"), std::string::npos)
      << "response payload did not contain PONG";
}
