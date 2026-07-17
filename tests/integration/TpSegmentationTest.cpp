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

#include <memory>
#include <string>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/config/config_linux.hpp"

using namespace someIp;
using namespace someIp::test;

class TpSegmentationTest : public MockHarnessTest {};

TEST_F(TpSegmentationTest, LargePayloadIsSegmentedAndReassembled) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  // just over one TxNPduLength (1392) so it segments into two PDUs
  const std::string body(1500, 'A');

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP_TP);
  server.listen_to_port(8000);

  std::atomic<bool> got_request{false};
  std::atomic<size_t> received_len{0};
  Callback method_cb([&](PackageRx &&p) {
    received_len = flatten(p).size();
    got_request = true;
  });
  Service svc(static_cast<uint16_t>(0x1234));
  svc.register_method(Method(static_cast<uint16_t>(1), std::move(method_cb)));
  server.register_service(std::move(svc));

  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP_TP);

  Package req(static_cast<uint16_t>(0x1234), static_cast<uint16_t>(1), body, server_ip, 8000);
  req.header._message_type = MessageType::REQUEST_NO_RETURN;
  client.fire_and_forget(std::move(req), TransportKind::UDP_TP);

  EXPECT_TRUE(wait_for(got_request)) << "server never received the reassembled request";
  EXPECT_GE(received_len.load(), body.size());
}
