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
#include "utils/AllocCounter.hpp"

#include <atomic>
#include <cstdio>
#include <memory>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/config/config_linux.hpp"

using namespace someIp;
using namespace someIp::test;

// measures C++ heap allocations on the steady-state request/response path
class NoHeapAfterInitTest : public MockHarnessTest {};

TEST_F(NoHeapAfterInitTest, RequestResponseSteadyStateAllocations) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
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
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.listen_to_port(config::SD_PORT);

  std::atomic<bool> got_response{false};
  auto response_cb = [&](PackageRx &&) { got_response = true; };

  // warm up one exchange so first-use lazy allocations are not counted
  {
    Package warm(0x1234, 1, "PING", server_ip, 8000);
    warm.header._message_type = MessageType::REQUEST;
    client.request_and_response(std::move(warm), response_cb, TransportKind::UDP);
    wait_for(got_response);
    got_response = false;
    got_request = false;
  }

  // measure one steady-state request/response round-trip
  alloc::arm();
  Package req(0x1234, 1, "PING", server_ip, 8000);
  req.header._message_type = MessageType::REQUEST;
  client.request_and_response(std::move(req), response_cb, TransportKind::UDP);
  ASSERT_TRUE(wait_for(got_response));
  alloc::disarm();

  const size_t allocations = alloc::count();
  std::printf("[no-heap] C++ heap allocations during one steady-state R/R: %zu\n", allocations);

  // TODO(no-heap) pool-back PacketBuffer to restore the zero-heap guarantee
  constexpr size_t STEADY_STATE_ALLOC_BUDGET = 6;
  EXPECT_LE(allocations, STEADY_STATE_ALLOC_BUDGET)
      << "steady-state C++ allocation regressed; see docs/embedded-suitability-review.md";
}
