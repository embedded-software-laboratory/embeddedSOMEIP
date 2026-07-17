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

using namespace someIp;
using namespace someIp::test;

namespace {

// one service with one request/response method that replies with a fixed string
Service make_service(ESomeIp &api, uint16_t service_id, uint16_t method_id,
                     const std::string &reply) {
  Callback cb([&api, reply](PackageRx &&p) {
    api.send_response(std::move(p), reply.c_str());
  });
  Service svc(service_id);
  svc.register_method(Method(method_id, std::move(cb)));
  return svc;
}

} // namespace

// two services on one server, requests must dispatch to the right method
class MultiServiceTest : public MockHarnessTest {};

TEST_F(MultiServiceTest, RequestsDispatchToCorrectService) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(8000);
  server.register_service(make_service(server, 0x1111, 1, "REPLY_A"));
  server.register_service(make_service(server, 0x2222, 1, "REPLY_B"));

  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.listen_to_port(8001);

  std::string reply_a, reply_b;
  std::atomic<bool> got_a{false}, got_b{false};

  Package req_a(0x1111, 1, "PING", server_ip, 8000);
  req_a.header._message_type = MessageType::REQUEST;
  client.request_and_response(std::move(req_a), [&](PackageRx &&p) { reply_a = flatten(p); got_a = true; }, TransportKind::UDP);
  ASSERT_TRUE(wait_for(got_a)) << "no response for service A";

  Package req_b(0x2222, 1, "PING", server_ip, 8000);
  req_b.header._message_type = MessageType::REQUEST;
  client.request_and_response(std::move(req_b), [&](PackageRx &&p) { reply_b = flatten(p); got_b = true; }, TransportKind::UDP);
  ASSERT_TRUE(wait_for(got_b)) << "no response for service B";
  EXPECT_NE(reply_a.find("REPLY_A"), std::string::npos);
  EXPECT_NE(reply_b.find("REPLY_B"), std::string::npos);
}

// an unknown method id must not be dispatched (no response, no crash)
TEST_F(MultiServiceTest, UnknownMethodIsNotDispatched) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(8000);
  server.register_service(make_service(server, 0x1111, 1, "REPLY_A"));

  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.listen_to_port(8001);

  std::atomic<bool> got_response{false};
  Package req(0x1111, 99, "PING", server_ip, 8000); // method 99 is unknown
  req.header._message_type = MessageType::REQUEST;
  client.request_and_response(std::move(req), [&](PackageRx &&) { got_response = true; }, TransportKind::UDP);

  EXPECT_FALSE(wait_for(got_response, 300)) << "unknown method should not produce a normal response";
}

// a request to an unregistered service id is dropped (no crash, no response)
TEST_F(MultiServiceTest, UnknownServiceIsNotDispatched) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(8000);
  server.register_service(make_service(server, 0x1111, 1, "REPLY_A"));

  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.listen_to_port(8001);

  std::atomic<bool> got_response{false};
  Package req(0x9999, 1, "PING", server_ip, 8000); // service 0x9999 is unregistered
  req.header._message_type = MessageType::REQUEST;
  client.request_and_response(std::move(req), [&](PackageRx &&) { got_response = true; }, TransportKind::UDP);

  EXPECT_FALSE(wait_for(got_response, 300)) << "unknown service should not produce a normal response";
}

// two Apis registering the same service id stay isolated
TEST_F(MultiServiceTest, SameServiceIdOnTwoApisIsIsolated) {
  someIp::IpAddr a_ip = make_ip("192.168.1.10");
  someIp::IpAddr b_ip = make_ip("192.168.1.11");
  someIp::IpAddr c_ip = make_ip("192.168.1.20");

  ESomeIp a(mock_populator());
  a.init(a_ip);
  a.set_transport_mode(TransportKind::UDP);
  a.listen_to_port(8000);
  a.register_service(make_service(a, 0x1234, 1, "FROM_A"));

  ESomeIp b(mock_populator());
  b.init(b_ip);
  b.set_transport_mode(TransportKind::UDP);
  b.listen_to_port(9000);
  b.register_service(make_service(b, 0x1234, 1, "FROM_B")); // same id, different eSomeIP

  ESomeIp client(mock_populator());
  client.init(c_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.listen_to_port(8001);

  std::string reply_a, reply_b;
  std::atomic<bool> got_a{false}, got_b{false};

  Package req_a(0x1234, 1, "PING", a_ip, 8000);
  req_a.header._message_type = MessageType::REQUEST;
  client.request_and_response(std::move(req_a), [&](PackageRx &&p) { reply_a = flatten(p); got_a = true; }, TransportKind::UDP);
  ASSERT_TRUE(wait_for(got_a)) << "no response from eSomeIP A";

  Package req_b(0x1234, 1, "PING", b_ip, 9000);
  req_b.header._message_type = MessageType::REQUEST;
  client.request_and_response(std::move(req_b), [&](PackageRx &&p) { reply_b = flatten(p); got_b = true; }, TransportKind::UDP);
  ASSERT_TRUE(wait_for(got_b)) << "no response from eSomeIP B";

  EXPECT_NE(reply_a.find("FROM_A"), std::string::npos);
  EXPECT_NE(reply_b.find("FROM_B"), std::string::npos);
}

// repeatedly create and destroy Apis with clean teardown and no state leak
TEST_F(MultiServiceTest, RepeatedApiCreateDestroy) {
  someIp::IpAddr s_ip = make_ip("192.168.1.10");
  someIp::IpAddr c_ip = make_ip("192.168.1.20");

  for (int iter = 0; iter < 4; ++iter) {
    ESomeIp server(mock_populator());
    server.init(s_ip);
    server.set_transport_mode(TransportKind::UDP);
    server.listen_to_port(8000);
    server.register_service(make_service(server, 0x1234, 1, "PONG"));

    ESomeIp client(mock_populator());
    client.init(c_ip);
    client.set_transport_mode(TransportKind::UDP);
    client.listen_to_port(8001);

    std::atomic<bool> got{false};
    Package req(0x1234, 1, "PING", s_ip, 8000);
    req.header._message_type = MessageType::REQUEST;
    client.request_and_response(std::move(req), [&](PackageRx &&) { got = true; }, TransportKind::UDP);
    ASSERT_TRUE(wait_for(got)) << "request/response failed on iteration " << iter;
    // both Apis destroyed here so threads joined and transports unregistered
  }
}
