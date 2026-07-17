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

// T4 desktop side, SD time to first notification
// Discovers once, then times subscribe to first field notification per sample.
// Leaving uses subscribe with TTL 0, wire-identical to STOP_SUBSCRIBE_EVENTGROUP.
// unsubscribe_all omits the endpoint option and the server rejects it.
// MAX_EVENT_PAYLOAD caps events at 64 bytes, so sizes are 16, 32 and 64.
// Pairs with STM32 SOMEIP_TESTCASE=3.

#include <chrono>
#include <string>
#include <thread>
#include <vector>

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
    printf("  ttfn: --samples=N --settle-ms=MS --csv=FILE (subscribe -> first notification, UDP)\n");
    return 0;
  }
  if (cfg.csv_path.empty()) cfg.csv_path = "T4_sd_ttfn.csv";
  int samples = cfg.samples_set ? cfg.num_samples : 100;

  printf("[T4] Subscribe->first-notification latency, %d samples/size (UDP)\n", samples);

  ESomeIp someIp;
  IpAddr client_ip = IpAddr::from_string(cfg.client_ip.c_str());
  someIp.init(client_ip);
  someIp.set_transport_mode(TransportKind::UDP);

  // rendezvous, signalled by the initial field notification
  latency::Waiter w;
  someIp.register_event_handler(cfg.service_id, example::FIELD_EVENT_ID,
                                [&w](PackageRx &&) { w.signal(); });

  someIp.enable_sd(client_ip);
  someIp.listen_to_port(cfg.client_port);

  // discover once, TTFN samples exercise only the subscription
  sd::SdServiceInfo info{cfg.service_id, cfg.instance_id,
                         static_cast<uint8_t>(cfg.major), cfg.minor};
  auto client = sd::pool_make<sd::SdClientService>(info);
  someIp.register_client_service(client);
  someIp.send_find_service(info);
  if (!latency::wait_for_phase(client, 10000)) {
    printf("[T4] ERROR: service not found via SD\n");
    return 1;
  }
  PoolPtr<sd::SdService> found = someIp.find_service(info);
  if (!found) {
    printf("[T4] ERROR: service not found via SD\n");
    return 1;
  }
  sd::SdEndpointInfo ep = found->get_endpoint();
  printf("[T4] Service found at %s:%u\n", ep._ip.c_str(), ep._port);

  // notifications are delivered to this UDP endpoint
  auto udp_ep = sd::pool_make<sd::Ipv4Option>(
      client_ip, cfg.client_port, sd::SdTransportProtocol::UDP, false);
  sd::OptionVec sub_options;
  sub_options.push_back(udp_ep);

  const sd::SdEventgroupInfo eg_join{cfg.service_id, cfg.instance_id,
                                     static_cast<uint8_t>(cfg.major), cfg.eventgroup_id};
  sd::SdEventgroupInfo eg_leave = eg_join;
  eg_leave._ttl = sd::SD_END_TTL; // wire-identical to STOP_SUBSCRIBE_EVENTGROUP

  latency::CsvSink csv(cfg.csv_path, "test,transport,payload_size,sample,ttfn_us");

  for (std::size_t si = 0; si < sizeof(latency::kFieldSizes) / sizeof(latency::kFieldSizes[0]); ++si) {
    std::size_t size = latency::kFieldSizes[si];

    // prime the field while unsubscribed so the next join measures cleanly
    Package set(cfg.service_id, example::FIELD_SET_ID, latency::make_payload(size),
                ep._ip, ep._port);
    set.header._message_type = MessageType::REQUEST_NO_RETURN;
    someIp.fire_and_forget(std::move(set), TransportKind::UDP);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<double> v;
    v.reserve(static_cast<std::size_t>(samples));
    int dropped = 0;
    for (int i = 0; i < samples; ++i) {
      w.reset();
      auto t0 = latency::clock::now();
      someIp.subscribe(eg_join, sub_options);
      if (w.wait_ms(1000)) {
        double us = std::chrono::duration<double, std::micro>(w.t1 - t0).count();
        v.push_back(us);
        csv.row("sd_ttfn,udp,%zu,%d,%.3f", size, i, us);
      } else {
        ++dropped;
        csv.row("sd_ttfn,udp,%zu,%d,%.3f", size, i, -1.0);
      }

      // leave the eventgroup so the next sample is a fresh join
      someIp.subscribe(eg_leave, sub_options);
      std::this_thread::sleep_for(std::chrono::milliseconds(cfg.settle_ms));
    }
    latency::print_summary("sd_ttfn", "udp", size, latency::compute_stats(v), dropped);
  }

  printf("[T4] Done, wrote %s\n", cfg.csv_path.c_str());
  return 0;
}
