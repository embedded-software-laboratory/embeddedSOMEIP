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

// T2 desktop side, SD time to first message
// Each sample builds a fresh ESomeIp, discovers, then does one request response.
// CSV records t_discover, t_first_response and ttfm.
// --sd-mode active multicasts a FindService, passive only listens for cyclic OfferService.
// Fresh instance per sample is required, the SD cache never expires.
// Pairs with STM32 SOMEIP_TESTCASE=2.

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/service_discovery/SdClientService.hpp"
#include "someIp/service_discovery/SdPool.hpp"
#include "someIp/service_discovery/SdService.hpp"
#include "someIp/utils/PoolTrace.hpp"
#include "common/cli.hpp"
#include "common/constants.hpp"
#include "common/latency.hpp"

using namespace someIp;

namespace {

double us_between(latency::clock::time_point a, latency::clock::time_point b) {
  return std::chrono::duration<double, std::micro>(b - a).count();
}

void print_stats_line(const char *label, const std::vector<double> &v) {
  latency::Stats s = latency::compute_stats(v);
  printf("[T2] %-16s n=%zu | min=%.1f mean=%.1f median=%.1f p95=%.1f p99=%.1f max=%.1f std=%.1f (us)\n",
         label, s.n, s.min, s.mean, s.median, s.p95, s.p99, s.max, s.stddev);
}

// live pool counts must return to baseline or the recreate loop leaks
void print_pool_live(int sample) {
  fprintf(stderr, "[T2][pools] sample=%d live: %s=%u %s=%u %s=%u %s=%u\n", sample,
          pool_id::name(pool_id::SD_ENTRY_OPTION), pool_trace_live(pool_id::SD_ENTRY_OPTION),
          pool_id::name(pool_id::SD_LARGE), pool_trace_live(pool_id::SD_LARGE),
          pool_id::name(pool_id::SD_SERVICE), pool_trace_live(pool_id::SD_SERVICE),
          pool_id::name(pool_id::TIMER), pool_trace_live(pool_id::TIMER));
}

} // namespace

int main(int argc, char *argv[]) {
  example::Config cfg;
  example::parse_args(argc, argv, cfg);
  if (cfg.show_help) {
    example::print_common_usage(argv[0]);
    printf("  ttfm: --sd-mode=active|passive --samples=N --settle-ms=MS --csv=FILE\n");
    return 0;
  }
  if (cfg.csv_path.empty()) cfg.csv_path = "T2_sd_ttfm.csv";
  // discovery plus settle per sample, so default lower than the sweeps
  int samples = cfg.samples_set ? cfg.num_samples : 50;
  bool active = (cfg.sd_mode == "active");

  printf("[T2] SD time-to-first-message, %d cold-start samples, sd-mode=%s (UDP)\n",
         samples, cfg.sd_mode.c_str());

  IpAddr client_ip = IpAddr::from_string(cfg.client_ip.c_str());
  sd::SdServiceInfo info{cfg.service_id, cfg.instance_id,
                         static_cast<uint8_t>(cfg.major), cfg.minor};

  // outlives each instance so a late response cannot signal a destroyed Waiter
  latency::Waiter w;
  std::string payload = latency::make_payload(16);

  latency::CsvSink csv(cfg.csv_path,
                       "test,transport,sd_mode,sample,t_discover_us,t_first_response_us,ttfm_us");
  std::vector<double> v_discover, v_response, v_ttfm;
  int failed = 0;

  for (int i = 0; i < samples; ++i) {
    double d_us = -1.0, r_us = -1.0, ttfm = -1.0;
    {
      // fresh instance means fresh SD state, scope end releases everything
      ESomeIp someIp;
      someIp.init(client_ip);
      someIp.set_transport_mode(TransportKind::UDP);
      someIp.enable_sd(client_ip);
      someIp.listen_to_port(cfg.client_port);

      auto client = sd::pool_make<sd::SdClientService>(info);

      auto t0 = latency::clock::now();
      someIp.register_client_service(client);
      if (active) someIp.send_find_service(info);

      if (latency::wait_for_phase(client, 5000)) {
        auto t1 = latency::clock::now();
        d_us = us_between(t0, t1);

        PoolPtr<sd::SdService> found = someIp.find_service(info); // cached now
        if (found) {
          sd::SdEndpointInfo ep = found->get_endpoint();
          if (i == 0)
            printf("[T2] Service found at %s:%u (first sample: %.1f us discovery)\n",
                   ep._ip.c_str(), ep._port, d_us);

          w.reset();
          Package req(cfg.service_id, cfg.method_id, payload, ep._ip, ep._port);
          req.header._message_type = MessageType::REQUEST;
          auto t2 = latency::clock::now();
          someIp.request_and_response(std::move(req), [&w](PackageRx &&) { w.signal(); },
                                      TransportKind::UDP);
          if (w.wait_ms(2000)) {
            r_us = us_between(t2, w.t1);
            ttfm = us_between(t0, w.t1);
          }
        }
      }
    }

    csv.row("sd_ttfm,udp,%s,%d,%.3f,%.3f,%.3f", cfg.sd_mode.c_str(), i, d_us, r_us, ttfm);
    if (ttfm >= 0) {
      v_discover.push_back(d_us);
      v_response.push_back(r_us);
      v_ttfm.push_back(ttfm);
    } else {
      ++failed;
      fprintf(stderr, "[T2] sample %d failed (%s)\n", i,
              d_us < 0 ? "discovery timeout" : "no response");
    }

    if (i % 10 == 0 || i == samples - 1) print_pool_live(i);
    std::this_thread::sleep_for(std::chrono::milliseconds(cfg.settle_ms));
  }

  printf("[T2] sd-mode=%s, %d/%d samples ok\n", cfg.sd_mode.c_str(),
         static_cast<int>(v_ttfm.size()), samples);
  print_stats_line("t_discover", v_discover);
  print_stats_line("t_first_response", v_response);
  print_stats_line("ttfm", v_ttfm);
  printf("[T2] Done, wrote %s\n", cfg.csv_path.c_str());
  return failed == samples ? 1 : 0;
}
