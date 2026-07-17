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

#include "utils/MockHarness.hpp"

#include <atomic>
#include <memory>
#include <string>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/config/config_linux.hpp"
#include "someIp/communication/LinuxUdpTransport.hpp"

using namespace someIp;
using namespace someIp::test;

namespace {

ESomeIp::TransportPopulator linux_udp_populator() {
  return [](TransportRegistry &reg) {
    reg.emplace_udp<LinuxUdpTransport>();
  };
}

constexpr uint16_t SERVICE_ID = 0x2002;
constexpr uint16_t METHOD_ID = 0x0001;
constexpr uint16_t SERVER_PORT = 18010;

Service make_echo_service(ESomeIp &api, std::atomic<bool> &got_request,
                          std::atomic<size_t> &req_len) {
  Callback cb([&api, &got_request, &req_len](PackageRx &&p) {
    req_len = flatten(p).size();
    got_request = true;
    api.send_response(std::move(p), "PONG");
  });
  Service svc(SERVICE_ID);
  svc.register_method(Method(METHOD_ID, std::move(cb)));
  return svc;
}

} // namespace

class LinuxLoopbackTest : public MockHarnessTest {};

TEST_F(LinuxLoopbackTest, RequestResponseOverRealUdp) {
  someIp::IpAddr ip = make_ip("127.0.0.1");

  ESomeIp api(linux_udp_populator());
  api.init(ip);
  api.set_transport_mode(TransportKind::UDP);
  api.listen_to_port(SERVER_PORT);

  std::atomic<bool> got_request{false};
  std::atomic<size_t> req_len{0};
  api.register_service(make_echo_service(api, got_request, req_len));

  std::atomic<bool> got_response{false};
  std::string response_payload;
  Package req(SERVICE_ID, METHOD_ID, "PING", ip, SERVER_PORT);
  req.header._message_type = MessageType::REQUEST;
  api.request_and_response(
      std::move(req),
      [&](PackageRx &&p) {
        response_payload = flatten(p);
        got_response = true;
      },
      TransportKind::UDP);

  EXPECT_TRUE(wait_for(got_request)) << "request not received over loopback";
  EXPECT_TRUE(wait_for(got_response)) << "response not received over loopback";
  EXPECT_NE(response_payload.find("PONG"), std::string::npos);
}

TEST_F(LinuxLoopbackTest, TpRequestResponseOverRealUdp) {
  someIp::IpAddr ip = make_ip("127.0.0.1");

  ESomeIp api(linux_udp_populator());
  api.init(ip);
  api.set_transport_mode(TransportKind::UDP_TP);
  api.listen_to_port(SERVER_PORT);

  std::atomic<bool> got_request{false};
  std::atomic<size_t> req_len{0};
  api.register_service(make_echo_service(api, got_request, req_len));

  const std::string body(3000, 'A'); // > TxNPduLength forces segmentation
  std::atomic<bool> got_response{false};
  Package req(SERVICE_ID, METHOD_ID, body, ip, SERVER_PORT);
  req.header._message_type = MessageType::REQUEST;
  api.request_and_response(
      std::move(req), [&](PackageRx &&) { got_response = true; }, TransportKind::UDP_TP);

  EXPECT_TRUE(wait_for(got_request, 4000)) << "reassembled request not received";
  EXPECT_GE(req_len.load(), body.size());
  EXPECT_TRUE(wait_for(got_response, 4000)) << "TP response not received";
}

TEST_F(LinuxLoopbackTest, FireAndForgetOverRealUdp) {
  someIp::IpAddr ip = make_ip("127.0.0.1");

  ESomeIp api(linux_udp_populator());
  api.init(ip);
  api.set_transport_mode(TransportKind::UDP);
  api.listen_to_port(SERVER_PORT);

  std::atomic<bool> got_request{false};
  std::atomic<size_t> req_len{0};
  api.register_service(make_echo_service(api, got_request, req_len));

  Package req(SERVICE_ID, METHOD_ID, "PING", ip, SERVER_PORT);
  req.header._message_type = MessageType::REQUEST;
  api.fire_and_forget(std::move(req), TransportKind::UDP);

  EXPECT_TRUE(wait_for(got_request)) << "fire-and-forget not received over loopback";
}

TEST_F(LinuxLoopbackTest, RepeatedRequestResponseOverRealUdp) {
  someIp::IpAddr ip = make_ip("127.0.0.1");

  ESomeIp api(linux_udp_populator());
  api.init(ip);
  api.set_transport_mode(TransportKind::UDP);
  api.listen_to_port(SERVER_PORT);

  std::atomic<bool> got_request{false};
  std::atomic<size_t> req_len{0};
  api.register_service(make_echo_service(api, got_request, req_len));

  constexpr int N = 8;
  std::atomic<int> responses{0};
  for (int i = 0; i < N; ++i) {
    std::atomic<bool> got{false};
    Package req(SERVICE_ID, METHOD_ID, "PING", ip, SERVER_PORT);
    req.header._message_type = MessageType::REQUEST;
    api.request_and_response(
        std::move(req),
        [&responses, &got](PackageRx &&) { responses++; got = true; },
        TransportKind::UDP);
    ASSERT_TRUE(wait_for(got)) << "no response for request " << i;
  }
  EXPECT_EQ(responses.load(), N);
}

TEST_F(LinuxLoopbackTest, LargePayloadRequestResponseOverRealUdp) {
  someIp::IpAddr ip = make_ip("127.0.0.1");

  ESomeIp api(linux_udp_populator());
  api.init(ip);
  api.set_transport_mode(TransportKind::UDP);
  api.listen_to_port(SERVER_PORT);

  std::atomic<bool> got_request{false};
  std::atomic<size_t> req_len{0};
  api.register_service(make_echo_service(api, got_request, req_len));

  const std::string body(1000, 'X'); // under one datagram (no TP)
  std::atomic<bool> got_response{false};
  Package req(SERVICE_ID, METHOD_ID, body, ip, SERVER_PORT);
  req.header._message_type = MessageType::REQUEST;
  api.request_and_response(
      std::move(req), [&](PackageRx &&) { got_response = true; }, TransportKind::UDP);

  EXPECT_TRUE(wait_for(got_request)) << "large request not received";
  EXPECT_GE(req_len.load(), body.size());
  EXPECT_TRUE(wait_for(got_response)) << "large response not received";
}

TEST_F(LinuxLoopbackTest, TwoApiRequestResponseOverRealUdp) {
  someIp::IpAddr ip = make_ip("127.0.0.1");
  constexpr uint16_t CLIENT_PORT = 18011;

  ESomeIp server(linux_udp_populator());
  server.init(ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(SERVER_PORT);
  std::atomic<bool> got_request{false};
  std::atomic<size_t> req_len{0};
  server.register_service(make_echo_service(server, got_request, req_len));

  ESomeIp client(linux_udp_populator());
  client.init(ip);
  client.set_transport_mode(TransportKind::UDP);
  client.listen_to_port(CLIENT_PORT);

  std::atomic<bool> got_response{false};
  std::string response_payload;
  Package req(SERVICE_ID, METHOD_ID, "PING", ip, SERVER_PORT);
  req.header._message_type = MessageType::REQUEST;
  client.request_and_response(
      std::move(req),
      [&](PackageRx &&p) { response_payload = flatten(p); got_response = true; },
      TransportKind::UDP);

  EXPECT_TRUE(wait_for(got_request)) << "server did not receive request from the other eSomeIP";
  EXPECT_TRUE(wait_for(got_response)) << "client did not receive the response";
  EXPECT_NE(response_payload.find("PONG"), std::string::npos);
}

TEST_F(LinuxLoopbackTest, TwoApiTpRequestResponseOverRealUdp) {
  someIp::IpAddr a_ip = make_ip("127.0.0.1");
  constexpr uint16_t CLIENT_PORT = 18011;

  ESomeIp server(linux_udp_populator());
  server.init(a_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(SERVER_PORT);
  std::atomic<bool> got_request{false};
  std::atomic<size_t> req_len{0};
  server.register_service(make_echo_service(server, got_request, req_len));

  ESomeIp client(linux_udp_populator());
  client.init(a_ip);
  client.set_transport_mode(TransportKind::UDP_TP);
  client.listen_to_port(CLIENT_PORT);

  const std::string body(3000, 'A'); // > MTU -> segmented
  std::atomic<bool> got_response{false};
  Package req(SERVICE_ID, METHOD_ID, body, a_ip, SERVER_PORT);
  req.header._message_type = MessageType::REQUEST;
  client.request_and_response(
      std::move(req), [&](PackageRx &&) { got_response = true; }, TransportKind::UDP_TP);

  EXPECT_TRUE(wait_for(got_request, 4000)) << "server did not reassemble the TP request from the other eSomeIP";
  EXPECT_GE(req_len.load(), body.size());
  EXPECT_TRUE(wait_for(got_response, 4000)) << "client did not receive the response";
}
