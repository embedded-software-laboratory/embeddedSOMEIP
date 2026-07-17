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

// T3 desktop side, notification round trip from field SET to event
// Discovers over SD, subscribes, then times each SET to its notification.
// MAX_EVENT_PAYLOAD caps events at 64 bytes, so sizes are 16, 32 and 64.
// Pairs with STM32 SOMEIP_TESTCASE=3.

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
#include "common/cli.hpp"
#include "common/constants.hpp"
#include "common/latency.hpp"

using namespace someIp;

int main(int argc, char *argv[]) {
  example::Config cfg;
  example::parse_args(argc, argv, cfg);
  if (cfg.show_help) {
    example::print_common_usage(argv[0]);
    printf("  latency: --samples=N --csv=FILE (field SET -> notification RTT, UDP)\n");
    return 0;
  }
  if (cfg.csv_path.empty()) cfg.csv_path = "T3_notify_rr.csv";

  printf("[T3] Field SET->notification latency, %d samples/size (UDP)\n", cfg.num_samples);

  ESomeIp someIp;
  IpAddr client_ip = IpAddr::from_string(cfg.client_ip.c_str());
  someIp.init(client_ip);
  someIp.set_transport_mode(TransportKind::UDP);

  // rendezvous, signalled by the field-change notification
  latency::Waiter w;
  someIp.register_event_handler(cfg.service_id, example::FIELD_EVENT_ID,
                                [&w](PackageRx &&) { w.signal(); });

  someIp.enable_sd(client_ip);
  someIp.listen_to_port(cfg.client_port);

  // discover the service
  sd::SdServiceInfo info{cfg.service_id, cfg.instance_id,
                           static_cast<uint8_t>(cfg.major), cfg.minor};
  auto client = sd::pool_make<sd::SdClientService>(info);
  someIp.register_client_service(client);
  int waited = 0;
  while (client->get_phase() != sd::SdClientPhase::MAIN &&
         client->get_phase() != sd::SdClientPhase::STOPPED && waited < 10000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    waited += 2;
  }
  someIp::PoolPtr<sd::SdService> found = someIp.find_service(info);
  if (!found) {
    printf("[T3] ERROR: service not found via SD\n");
    return 1;
  }
  sd::SdEndpointInfo ep = found->get_endpoint();
  IpAddr server_ip = ep._ip;
  uint16_t server_port = ep._port;
  printf("[T3] Service found at %s:%u\n", server_ip.c_str(), server_port);

  // subscribe with our UDP endpoint so notifications come back here
  auto udp_ep = sd::pool_make<sd::Ipv4Option>(
      client_ip, cfg.client_port, sd::SdTransportProtocol::UDP, false);
  sd::OptionVec sub_options;
  sub_options.push_back(udp_ep);
  someIp.subscribe(sd::SdEventgroupInfo{cfg.service_id, cfg.instance_id,
                                          static_cast<uint8_t>(cfg.major),
                                          cfg.eventgroup_id},
                   sub_options);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));  // let SUBSCRIBE settle
  printf("[T3] Subscribed to eventgroup 0x%04X\n", cfg.eventgroup_id);

  latency::CsvWriter csv(cfg.csv_path);

  auto send_once = [&](const std::string &payload, int idx) -> double {
    // vary a byte each sample so the on-change field notification always fires
    std::string p = payload;
    if (!p.empty()) p[0] = static_cast<char>(idx & 0xFF);
    w.reset();
    Package set(cfg.service_id, example::FIELD_SET_ID, p, server_ip, server_port);
    set.header._message_type = MessageType::REQUEST_NO_RETURN;
    auto t0 = latency::clock::now();
    someIp.fire_and_forget(std::move(set), TransportKind::UDP);
    if (!w.wait_ms(1000)) return -1.0;
    return std::chrono::duration<double, std::micro>(w.t1 - t0).count();
  };

  latency::run_sweep(csv, "notify_rr", "udp", latency::kFieldSizes,
                     sizeof(latency::kFieldSizes) / sizeof(latency::kFieldSizes[0]),
                     cfg.num_samples, send_once);

  printf("[T3] Done, wrote %s\n", cfg.csv_path.c_str());
  return 0;
}
