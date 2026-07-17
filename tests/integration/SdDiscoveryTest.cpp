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

#include "someIp/service_discovery/SdPool.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include "utils/MockHarness.hpp"

#include <memory>
#include <vector>

#include "someIp/ESomeIp.hpp"
#include "Def.hpp"
#include "SdService.hpp"
#include "SdClientService.hpp"
#include "Ipv4Option.hpp"
#include "LoadBalancingOption.hpp"
#include "Option.hpp"

using namespace someIp;
using namespace someIp::test;

namespace {

constexpr uint16_t SERVICE_ID = 0x1234;
constexpr uint16_t INSTANCE_ID = 0x0001;
constexpr uint8_t MAJOR = 0x01;
constexpr uint32_t MINOR = 0x00000001;
constexpr uint16_t SERVER_PORT = 8000;
constexpr uint16_t CLIENT_PORT = 8001;

} // namespace

// client FIND state machine discovers a server's offered endpoint
class SdDiscoveryTest : public MockHarnessTest {};

TEST_F(SdDiscoveryTest, ClientDiscoversOfferedServiceEndpoint) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(SERVER_PORT);
  server.enable_sd(server_ip);

  sd::SdServiceInfo sinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  sinfo._src_port = SERVER_PORT;
  sinfo._weight = 10;
  sinfo._priority = 5;
  auto sd_service = someIp::sd::pool_make_service<someIp::sd::SdService>(sinfo);
  sd::OptionVec options;
  options.push_back(someIp::sd::pool_make<someIp::sd::LoadBalancingOption>(sinfo._weight, sinfo._priority, true));
  options.push_back(someIp::sd::pool_make<someIp::sd::Ipv4Option>(server_ip, SERVER_PORT,
                                                      sd::SdTransportProtocol::UDP));
  sd_service->update_options(options);
  server.offer_service(sd_service, server_ip, SERVER_PORT);

  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.listen_to_port(CLIENT_PORT);
  client.enable_sd(client_ip);

  sd::SdServiceInfo cinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  auto client_service = someIp::sd::pool_make<someIp::sd::SdClientService>(cinfo);
  client.register_client_service(client_service);

  // wait for the client FIND state machine to settle
  bool settled = wait_until([&] {
    auto phase = client_service->get_phase();
    return phase == sd::SdClientPhase::MAIN || phase == sd::SdClientPhase::STOPPED;
  });
  ASSERT_TRUE(settled) << "client FIND state machine never left INITIAL/REPETITION";

  auto found = client.find_service(cinfo);
  ASSERT_NE(found, nullptr) << "service not discovered via SD";
  sd::SdEndpointInfo endpoint = found->get_endpoint();
  EXPECT_EQ(endpoint._port, SERVER_PORT);

  server.stop_all_services();
}
