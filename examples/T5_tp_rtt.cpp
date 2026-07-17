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

// T5 desktop side, SOME/IP-TP round trip for large payloads without SD
// Records RTT and two-way goodput for payloads spanning several TP segments.
// Default sizes are 2048, 4096 and 8192, capped by MAX_TP_RX_MESSAGE.
// Pairs with STM32 SOMEIP_TESTCASE=5.

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Package.hpp"
#include "common/cli.hpp"
#include "common/constants.hpp"
#include "common/latency.hpp"

using namespace someIp;

int main(int argc, char *argv[]) {
  example::Config cfg;
  example::parse_args(argc, argv, cfg);
  if (cfg.show_help) {
    example::print_common_usage(argv[0]);
    printf("  tp: --server-ip=IP --sizes=2048,4096,8192 --samples=N --csv=FILE\n");
    return 0;
  }
  if (cfg.csv_path.empty()) cfg.csv_path = "T5_tp_rtt.csv";
  int samples = cfg.samples_set ? cfg.num_samples : 100;

  std::vector<size_t> sizes = example::parse_size_list(cfg.sizes);
  if (sizes.empty()) sizes = {2048, 4096, 8192};

  printf("[T5] SOME/IP-TP request/response RTT to %s:%u, %d samples/size (UDP-TP)\n",
         cfg.server_ip.c_str(), cfg.server_port, samples);

  ESomeIp someIp;
  IpAddr client_ip = IpAddr::from_string(cfg.client_ip.c_str());
  someIp.init(client_ip);
  someIp.set_transport_mode(TransportKind::UDP_TP);
  someIp.listen_to_port(cfg.client_port);

  IpAddr server_ip = IpAddr::from_string(cfg.server_ip.c_str());

  // one rendezvous reused across all closed-loop samples
  latency::Waiter w;
  Callback on_response = [&w](PackageRx &&) { w.signal(); };

  latency::CsvSink csv(cfg.csv_path,
                       "test,transport,payload_size,sample,rtt_us,goodput_mbps");

  for (size_t size : sizes) {
    std::string payload = latency::make_payload(size);
    std::vector<double> v;
    v.reserve(static_cast<std::size_t>(samples));
    int dropped = 0;
    for (int i = 0; i < samples; ++i) {
      w.reset();
      Package req(cfg.service_id, cfg.method_id, payload, server_ip, cfg.server_port);
      req.header._message_type = MessageType::REQUEST;
      auto t0 = latency::clock::now();
      someIp.request_and_response(std::move(req), on_response, TransportKind::UDP_TP);
      // a lost segment loses the whole message, so allow a generous timeout
      if (!w.wait_ms(5000)) {
        ++dropped;
        continue;
      }
      double rtt_us = std::chrono::duration<double, std::micro>(w.t1 - t0).count();
      // payload crosses the wire twice (request + echoed response)
      double goodput_mbps = (2.0 * 8.0 * static_cast<double>(size)) / rtt_us;
      v.push_back(rtt_us);
      csv.row("tp_rtt,udp_tp,%zu,%d,%.3f,%.3f", size, i, rtt_us, goodput_mbps);
    }
    latency::print_summary("tp_rtt", "udp_tp", size, latency::compute_stats(v), dropped);
  }

  printf("[T5] Done, wrote %s\n", cfg.csv_path.c_str());
  return 0;
}
