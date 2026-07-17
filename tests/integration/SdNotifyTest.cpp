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

#include <atomic>
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
constexpr uint16_t EVENT_ID = 0x8001;
constexpr uint16_t EVENTGROUP_ID = 0x0001;
constexpr uint16_t SERVER_PORT = 8000;
constexpr uint16_t CLIENT_PORT = 8001;

someIp::PoolPtr<sd::SdService> make_offered_service(ESomeIp &server, someIp::IpAddr server_ip) {
  sd::SdServiceInfo sinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  sinfo._src_port = SERVER_PORT;
  auto sd_service = someIp::sd::pool_make_service<someIp::sd::SdService>(sinfo);
  server.register_sd_service(sd_service);
  sd_service->register_event(EVENT_ID, EVENTGROUP_ID);
  sd::OptionVec options;
  options.push_back(someIp::sd::pool_make<someIp::sd::Ipv4Option>(server_ip, SERVER_PORT, sd::SdTransportProtocol::UDP));
  sd_service->update_options(options);
  server.offer_service(sd_service, server_ip, SERVER_PORT);
  return sd_service;
}

void client_discover_and_subscribe(ESomeIp &client, someIp::IpAddr client_ip,
                                   someIp::PoolPtr<sd::SdClientService> &cs) {
  sd::SdServiceInfo cinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  cs = someIp::sd::pool_make<someIp::sd::SdClientService>(cinfo);
  client.register_client_service(cs);
  // subscribe needs the remote service in the registry first
  wait_until([&] { return client.find_service(cinfo) != nullptr; });
  // endpoint tells the server where to send notifications
  auto endpoint = someIp::sd::pool_make<someIp::sd::Ipv4Option>(client_ip, CLIENT_PORT, sd::SdTransportProtocol::UDP, false);
  client.subscribe(sd::SdEventgroupInfo{SERVICE_ID, INSTANCE_ID, MAJOR, EVENTGROUP_ID},
                   sd::OptionVec{endpoint});
}

} // namespace

class SdNotifyTest : public MockHarnessTest {};

TEST_F(SdNotifyTest, SubscribeThenNotifyDeliversEvent) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(SERVER_PORT);
  server.enable_sd(server_ip);
  auto sd_service = make_offered_service(server, server_ip);

  std::atomic<int> notifications{0};
  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.register_event_handler(SERVICE_ID, EVENT_ID, [&notifications](PackageRx &&) { notifications++; });
  client.listen_to_port(CLIENT_PORT);
  client.enable_sd(client_ip);

  someIp::PoolPtr<sd::SdClientService> cs;
  client_discover_and_subscribe(client, client_ip, cs);

  sd::SdServiceInfo sinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  bool subscribed = wait_until([&] {
    return !sd_service->get_subscribers_of_eventgroup(EVENTGROUP_ID).empty();
  });
  ASSERT_TRUE(subscribed) << "server never registered the subscriber";

  server.set_event_payload(sinfo, EVENT_ID, "payload-1");
  // re-notify to cover the subscribe/notify race
  bool delivered = wait_until([&] {
    server.notify_eventgroup(sinfo, EVENTGROUP_ID);
    return notifications.load() > 0;
  });
  EXPECT_TRUE(delivered) << "subscriber never received the notification";

  server.stop_all_services();
}

TEST_F(SdNotifyTest, CyclicUpdateDeliversPeriodicNotifications) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(SERVER_PORT);
  server.enable_sd(server_ip);
  auto sd_service = make_offered_service(server, server_ip);

  std::atomic<int> notifications{0};
  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.register_event_handler(SERVICE_ID, EVENT_ID, [&notifications](PackageRx &&) { notifications++; });
  client.listen_to_port(CLIENT_PORT);
  client.enable_sd(client_ip);

  someIp::PoolPtr<sd::SdClientService> cs;
  client_discover_and_subscribe(client, client_ip, cs);

  sd::SdServiceInfo sinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  ASSERT_TRUE(wait_until([&] {
    return !sd_service->get_subscribers_of_eventgroup(EVENTGROUP_ID).empty();
  })) << "server never registered the subscriber";

  server.set_event_payload(sinfo, EVENT_ID, "cyclic-payload");
  server.start_cyclic_update(sinfo, EVENTGROUP_ID, 20);

  bool got_periodic = wait_until([&] { return notifications.load() >= 2; }, 4000);
  server.stop_cyclic_update(sinfo, EVENTGROUP_ID);

  EXPECT_TRUE(got_periodic) << "cyclic update did not deliver repeated notifications";

  server.stop_all_services();
}

TEST_F(SdNotifyTest, StopCyclicUpdateHaltsNotifications) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");
  someIp::IpAddr client_ip = make_ip("192.168.1.20");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(SERVER_PORT);
  server.enable_sd(server_ip);
  auto sd_service = make_offered_service(server, server_ip);

  std::atomic<int> notifications{0};
  ESomeIp client(mock_populator());
  client.init(client_ip);
  client.set_transport_mode(TransportKind::UDP);
  client.register_event_handler(SERVICE_ID, EVENT_ID, [&notifications](PackageRx &&) { notifications++; });
  client.listen_to_port(CLIENT_PORT);
  client.enable_sd(client_ip);

  someIp::PoolPtr<sd::SdClientService> cs;
  client_discover_and_subscribe(client, client_ip, cs);

  sd::SdServiceInfo sinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  ASSERT_TRUE(wait_until([&] {
    return !sd_service->get_subscribers_of_eventgroup(EVENTGROUP_ID).empty();
  })) << "server never registered the subscriber";

  server.set_event_payload(sinfo, EVENT_ID, "p");
  server.start_cyclic_update(sinfo, EVENTGROUP_ID, 20);
  ASSERT_TRUE(wait_until([&] { return notifications.load() >= 2; }, 4000));

  server.stop_cyclic_update(sinfo, EVENTGROUP_ID);
  int after_stop = notifications.load();
  // allow one in-flight notification
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_LE(notifications.load() - after_stop, 1) << "notifications kept arriving after stop_cyclic_update";

  server.stop_all_services();
}

// regression for the get_subscribers .at() abort
TEST_F(SdNotifyTest, NotifyWithoutSubscribersIsSafe) {
  someIp::IpAddr server_ip = make_ip("192.168.1.10");

  ESomeIp server(mock_populator());
  server.init(server_ip);
  server.set_transport_mode(TransportKind::UDP);
  server.listen_to_port(SERVER_PORT);
  server.enable_sd(server_ip);
  auto sd_service = make_offered_service(server, server_ip);

  sd::SdServiceInfo sinfo{SERVICE_ID, INSTANCE_ID, MAJOR, MINOR};
  server.set_event_payload(sinfo, EVENT_ID, "no-subscribers");
  server.notify_eventgroup(sinfo, EVENTGROUP_ID); // must not crash
  server.start_cyclic_update(sinfo, EVENTGROUP_ID, 20);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  server.stop_cyclic_update(sinfo, EVENTGROUP_ID);

  SUCCEED();
  server.stop_all_services();
}
