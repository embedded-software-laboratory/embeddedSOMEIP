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

// B1 desktop side, plain UDP request response latency
// No SOME/IP, pairs with the STM32 UDP_BASELINE echo image.
// Sweeps the same sizes as T1.

#include <cstdio>
#include <string>

#include "common/cli.hpp"
#include "common/constants.hpp"
#include "common/latency.hpp"
#include "common/raw_udp.hpp"

int main(int argc, char *argv[]) {
  example::Config cfg;
  example::parse_args(argc, argv, cfg);
  if (cfg.show_help) {
    example::print_common_usage(argv[0]);
    printf("  baseline: plain-UDP RR to the echo image --server-ip=IP --samples=N --csv=FILE\n");
    return 0;
  }
  if (cfg.csv_path.empty()) cfg.csv_path = "B1_udp_rr.csv";

  printf("[B1] Plain-UDP request/response latency to %s:%u, %d samples/size\n",
         cfg.server_ip.c_str(), cfg.server_port, cfg.num_samples);

  rawudp::Endpoint ep;
  if (!ep.open(cfg.client_ip, cfg.client_port, cfg.server_ip, cfg.server_port)) {
    printf("[B1] ERROR: could not open UDP socket\n");
    return 1;
  }

  latency::CsvWriter csv(cfg.csv_path);
  auto send_once = [&](const std::string &payload, int /*idx*/) -> double {
    return ep.send_once(payload);
  };

  latency::run_sweep(csv, "rr_baseline", "udp", latency::kSweepSizes,
                     sizeof(latency::kSweepSizes) / sizeof(latency::kSweepSizes[0]),
                     cfg.num_samples, send_once);

  printf("[B1] Done, wrote %s\n", cfg.csv_path.c_str());
  return 0;
}
