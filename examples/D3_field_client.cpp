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

// D3 client, field and eventgroup over SD on UDP
// Subscribes, SETs the field, GETs it back, then stops the server.

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/service_discovery/Ipv4Option.hpp"
#include "someIp/service_discovery/SdClientService.hpp"
#include "someIp/service_discovery/SdPool.hpp"
#include "someIp/service_discovery/SdService.hpp"
#include "common/constants.hpp"

using namespace someIp;

static std::atomic<int> g_notifications{0};
static std::atomic<int> g_responses{0};

int main() {
  printf("[D3] Field/event client (SD, UDP)\n");

  ESomeIp someIp;
  IpAddr client_ip = IpAddr::from_string(example::CLIENT_IP);
  someIp.init(client_ip);
  someIp.set_transport_mode(TransportKind::UDP);

  someIp.register_event_handler(example::SERVICE_ID, example::FIELD_EVENT_ID,
                                [](PackageRx &&p) {
    std::string v(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
    printf("[D3] Notification: %s\n", v.c_str());
    g_notifications.fetch_add(1, std::memory_order_relaxed);
  });

  someIp.enable_sd(client_ip);
  someIp.listen_to_port(example::CLIENT_PORT);

  sd::SdServiceInfo info{example::SERVICE_ID, example::INSTANCE_ID,
                         example::MAJOR_VERSION, example::MINOR_VERSION};
  auto client = sd::pool_make<sd::SdClientService>(info);
  someIp.register_client_service(client);
  int waited = 0;
  while (client->get_phase() != sd::SdClientPhase::MAIN &&
         client->get_phase() != sd::SdClientPhase::STOPPED && waited < 10000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    waited += 2;
  }
  PoolPtr<sd::SdService> found = someIp.find_service(info);
  if (!found) {
    printf("[D3] ERROR: service not found via SD\n");
    return 1;
  }
  sd::SdEndpointInfo ep = found->get_endpoint();
  IpAddr server_ip = ep._ip;
  uint16_t server_port = ep._port;
  printf("[D3] Service found at %s:%u\n", server_ip.c_str(), server_port);

  auto udp_ep = sd::pool_make<sd::Ipv4Option>(
      client_ip, example::CLIENT_PORT, sd::SdTransportProtocol::UDP, false);
  sd::OptionVec sub_options;
  sub_options.push_back(udp_ep);
  someIp.subscribe(sd::SdEventgroupInfo{example::SERVICE_ID, example::INSTANCE_ID,
                                        example::MAJOR_VERSION, example::EVENTGROUP_ID},
                   sub_options);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  // the server's initial notify should have arrived by now
  printf("[D3] Subscribed (%d notifications so far, expected 1 initial)\n",
         g_notifications.load());

  // each SET triggers a notification
  for (int i = 1; i <= 3; ++i) {
    std::string value = "value-" + std::to_string(i);
    Package set(example::SERVICE_ID, example::FIELD_SET_ID, value, server_ip, server_port);
    set.header._message_type = MessageType::REQUEST_NO_RETURN;
    someIp.fire_and_forget(std::move(set), TransportKind::UDP);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  Package get(example::SERVICE_ID, example::FIELD_GET_ID, " ", server_ip, server_port);
  get.header._message_type = MessageType::REQUEST;
  someIp.request_and_response(std::move(get), [](PackageRx &&p) {
    std::string v(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
    printf("[D3] GET response: %s\n", v.c_str());
    g_responses.fetch_add(1, std::memory_order_relaxed);
  }, TransportKind::UDP);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  Package bye(example::SERVICE_ID, example::FIELD_SET_ID, "shutdown", server_ip, server_port);
  bye.header._message_type = MessageType::REQUEST_NO_RETURN;
  someIp.fire_and_forget(std::move(bye), TransportKind::UDP);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  printf("[D3] Done (%d notifications, %d GET responses)\n",
         g_notifications.load(), g_responses.load());
  return 0;
}
