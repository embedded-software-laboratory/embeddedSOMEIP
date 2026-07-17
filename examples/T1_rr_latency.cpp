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

// T1 desktop side, direct request response latency without SD
// Sweeps payload sizes 16 to 1024 over SWEEP_TRANSPORT, writes per-sample CSV.
// Pairs with STM32 SOMEIP_TESTCASE=1.

#include <chrono>
#include <string>
#include <thread>

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
    printf("  latency: --server-ip=IP --samples=N --csv=FILE\n");
    return 0;
  }
  if (cfg.csv_path.empty()) cfg.csv_path = "T1_rr_latency.csv";

  TransportKind tk = latency::selected_transport();
  const char *tname = latency::transport_name(tk);
  printf("[T1] Direct request/response latency to %s:%u over %s, %d samples/size\n",
         cfg.server_ip.c_str(), cfg.server_port, tname, cfg.num_samples);

  ESomeIp someIp;
  IpAddr client_ip = IpAddr::from_string(cfg.client_ip.c_str());
  someIp.init(client_ip);
  someIp.set_transport_mode(tk);
  someIp.listen_to_port(cfg.client_port);

  IpAddr server_ip = IpAddr::from_string(cfg.server_ip.c_str());

  TcpConnection *conn = nullptr;
  if (tk == TransportKind::TCP) {
    conn = someIp.connect_to_tcp_server(server_ip, cfg.server_port);
    int waited = 0;
    while (!someIp.is_tcp_connected(server_ip, cfg.server_port) && waited < 2000) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      waited += 10;
    }
    if (!someIp.is_tcp_connected(server_ip, cfg.server_port)) {
      printf("[T1] ERROR: TCP connect to %s:%u failed\n", cfg.server_ip.c_str(), cfg.server_port);
      return 1;
    }
  }

  // one rendezvous reused across all closed-loop samples
  latency::Waiter w;
  Callback on_response = [&w](PackageRx &&) { w.signal(); };
  latency::CsvWriter csv(cfg.csv_path);

  auto send_once = [&](const std::string &payload, int /*idx*/) -> double {
    w.reset();
    Package req(cfg.service_id, cfg.method_id, payload, server_ip, cfg.server_port);
    req.header._message_type = MessageType::REQUEST;
    auto t0 = latency::clock::now();
    someIp.request_and_response(std::move(req), on_response, tk, conn);
    if (!w.wait_ms(1000)) return -1.0;  // dropped
    return std::chrono::duration<double, std::micro>(w.t1 - t0).count();
  };

  latency::run_sweep(csv, "rr", tname, latency::kSweepSizes,
                     sizeof(latency::kSweepSizes) / sizeof(latency::kSweepSizes[0]),
                     cfg.num_samples, send_once);

  printf("[T1] Done, wrote %s\n", cfg.csv_path.c_str());
  return 0;
}
