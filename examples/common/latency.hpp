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

#ifndef EXAMPLES_COMMON_LATENCY_HPP
#define EXAMPLES_COMMON_LATENCY_HPP

// Header-only latency helpers for the desktop SOME/IP clients.
// One desktop steady_clock times every round trip, so no clock sync.
// SWEEP_TRANSPORT must match the STM32 server's SOMEIP_TRANSPORT.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "someIp/config/communication_config.hpp" // someIp::TransportKind
#include "someIp/service_discovery/SdClientService.hpp"
#include "someIp/utils/PoolPtr.hpp"

// --- compile-time transport switch -----------------------------------------
#define SWEEP_UDP 1
#define SWEEP_TCP 2
#ifndef SWEEP_TRANSPORT
#define SWEEP_TRANSPORT SWEEP_UDP
#endif

namespace latency {

using clock = std::chrono::steady_clock;

// payload sizes swept by the method-based tests (single UDP datagram, < MTU)
inline constexpr std::size_t kSweepSizes[] = {16, 32, 64, 128, 256, 512, 1024};
// field/event payloads are capped by MAX_EVENT_PAYLOAD (64 bytes)
inline constexpr std::size_t kFieldSizes[] = {16, 32, 64};

// deterministic printable payload of n bytes
inline std::string make_payload(std::size_t n) {
  std::string s(n, '\0');
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<char>('A' + (i % 26));
  return s;
}

inline const char *transport_name(someIp::TransportKind t) {
  switch (t) {
    case someIp::TransportKind::TCP: return "tcp";
    case someIp::TransportKind::UDP_TP: return "udp_tp";
    default: return "udp";
  }
}

// the transport to use, per the SWEEP_TRANSPORT compile-time switch
inline someIp::TransportKind selected_transport() {
#if SWEEP_TRANSPORT == SWEEP_TCP
  return someIp::TransportKind::TCP;
#else
  return someIp::TransportKind::UDP;
#endif
}

// --- one-outstanding-request rendezvous between the send loop and the RX
// worker thread that delivers the reply/notification ------------------------
struct Waiter {
  std::mutex m;
  std::condition_variable cv;
  bool got = false;
  clock::time_point t1;

  void reset() {
    std::lock_guard<std::mutex> l(m);
    got = false;
  }
  // called from the SOME/IP RX worker when the reply arrives
  void signal() {
    clock::time_point now = clock::now();
    {
      std::lock_guard<std::mutex> l(m);
      t1 = now;
      got = true;
    }
    cv.notify_one();
  }
  // returns true if signalled within ms, false on timeout (dropped packet)
  bool wait_ms(int ms) {
    std::unique_lock<std::mutex> l(m);
    return cv.wait_for(l, std::chrono::milliseconds(ms), [&] { return got; });
  }
};

// --- summary statistics -----------------------------------------------------
struct Stats {
  std::size_t n = 0;
  double min = 0, max = 0, mean = 0, median = 0, p95 = 0, p99 = 0, stddev = 0;
};

inline double percentile(const std::vector<double> &sorted, double p) {
  if (sorted.empty()) return 0.0;
  double idx = p * static_cast<double>(sorted.size() - 1);
  std::size_t lo = static_cast<std::size_t>(idx);
  std::size_t hi = std::min(lo + 1, sorted.size() - 1);
  double frac = idx - static_cast<double>(lo);
  return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

inline Stats compute_stats(std::vector<double> v) {
  Stats s;
  s.n = v.size();
  if (v.empty()) return s;
  std::sort(v.begin(), v.end());
  s.min = v.front();
  s.max = v.back();
  double sum = 0;
  for (double x : v) sum += x;
  s.mean = sum / static_cast<double>(v.size());
  double var = 0;
  for (double x : v) {
    double d = x - s.mean;
    var += d * d;
  }
  s.stddev = std::sqrt(var / static_cast<double>(v.size()));
  s.median = percentile(v, 0.50);
  s.p95 = percentile(v, 0.95);
  s.p99 = percentile(v, 0.99);
  return s;
}

inline void print_summary(const char *test, const char *transport,
                          std::size_t size, const Stats &s, int dropped) {
  std::printf(
      "[%s][%s] size=%4zu n=%zu dropped=%d | min=%.1f mean=%.1f median=%.1f "
      "p95=%.1f p99=%.1f max=%.1f std=%.1f (us)\n",
      test, transport, size, s.n, dropped, s.min, s.mean, s.median, s.p95,
      s.p99, s.max, s.stddev);
}

// one payload size's outcome, collected for the end-of-experiment table
struct SummaryRow {
  std::size_t size;
  Stats stats;
  int dropped;
};

// dropped samples are shown for context but excluded from the stats
inline void print_summary_table(const char *test, const char *transport,
                                const std::vector<SummaryRow> &rows) {
  std::printf("\n=== summary [%s] (%s): min/mean/p99/max per payload size, "
              "received samples only ===\n",
              test, transport);
  std::printf("%6s %8s %6s %10s %10s %10s %10s\n", "size", "n", "drop", "min",
              "mean", "p99", "max");
  for (const SummaryRow &r : rows) {
    std::printf("%6zu %8zu %6d %10.1f %10.1f %10.1f %10.1f\n", r.size,
                r.stats.n, r.dropped, r.stats.min, r.stats.mean, r.stats.p99,
                r.stats.max);
  }
  std::printf("(microseconds)\n");
}

// --- CSV writer -------------------------------------------------------------
class CsvWriter {
 public:
  explicit CsvWriter(const std::string &path, bool owd_col = false)
      : owd_(owd_col) {
    if (path.empty()) {
      f_ = stdout;
    } else {
      f_ = std::fopen(path.c_str(), "w");
      owns_ = (f_ != nullptr);
    }
    if (f_) {
      std::fprintf(f_, owd_ ? "test,transport,payload_size,sample,rtt_us,owd_est_us\n"
                            : "test,transport,payload_size,sample,rtt_us\n");
    }
  }
  ~CsvWriter() {
    if (f_ && owns_) std::fclose(f_);
  }
  CsvWriter(const CsvWriter &) = delete;
  CsvWriter &operator=(const CsvWriter &) = delete;

  bool ok() const { return f_ != nullptr; }

  void row(const char *test, const char *transport, std::size_t size,
           int sample, double rtt_us) {
    if (f_) std::fprintf(f_, "%s,%s,%zu,%d,%.3f\n", test, transport, size, sample, rtt_us);
  }
  void row(const char *test, const char *transport, std::size_t size,
           int sample, double rtt_us, double owd_us) {
    if (f_)
      std::fprintf(f_, "%s,%s,%zu,%d,%.3f,%.3f\n", test, transport, size, sample,
                   rtt_us, owd_us);
  }

 private:
  std::FILE *f_ = nullptr;
  bool owns_ = false;
  bool owd_ = false;
};

// --- free-form CSV sink -------------------------------------------------------
// for tests whose columns do not fit CsvWriter
class CsvSink {
 public:
  CsvSink(const std::string &path, const char *header) {
    if (path.empty()) {
      f_ = stdout;
    } else {
      f_ = std::fopen(path.c_str(), "w");
      owns_ = (f_ != nullptr);
    }
    if (f_) std::fprintf(f_, "%s\n", header);
  }
  ~CsvSink() {
    if (f_ && owns_) std::fclose(f_);
  }
  CsvSink(const CsvSink &) = delete;
  CsvSink &operator=(const CsvSink &) = delete;

  bool ok() const { return f_ != nullptr; }

  __attribute__((format(printf, 2, 3))) void row(const char *fmt, ...) {
    if (!f_) return;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f_, fmt, args);
    va_end(args);
    std::fputc('\n', f_);
  }

 private:
  std::FILE *f_ = nullptr;
  bool owns_ = false;
};

// --- SD client-phase rendezvous ----------------------------------------------
// the 200 us poll keeps quantization error far below the SD timing measured
inline bool wait_for_phase(const someIp::PoolPtr<someIp::sd::SdClientService> &client,
                           int timeout_ms, int poll_us = 200) {
  auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
  for (;;) {
    auto phase = client->get_phase();
    if (phase == someIp::sd::SdClientPhase::MAIN) return true;
    if (phase == someIp::sd::SdClientPhase::STOPPED) return false;
    if (clock::now() >= deadline) return false;
    std::this_thread::sleep_for(std::chrono::microseconds(poll_us));
  }
}

// --- generic RTT sweep ------------------------------------------------------
// send_once returns the RTT in microseconds, negative when dropped
template <class SendOnce>
void run_sweep(CsvWriter &csv, const char *test, const char *tname,
               const std::size_t *sizes, std::size_t n_sizes, int samples,
               SendOnce &&send_once) {
  std::vector<SummaryRow> summary;
  summary.reserve(n_sizes);
  for (std::size_t si = 0; si < n_sizes; ++si) {
    std::string payload = make_payload(sizes[si]);
    std::vector<double> v;
    v.reserve(static_cast<std::size_t>(samples));
    int dropped = 0;
    for (int i = 0; i < samples; ++i) {
      double us = send_once(payload, i);
      if (us < 0) {
        ++dropped;
        continue;
      }
      v.push_back(us);
      csv.row(test, tname, sizes[si], i, us);
    }
    Stats s = compute_stats(v);
    print_summary(test, tname, sizes[si], s, dropped);
    summary.push_back(SummaryRow{sizes[si], s, dropped});
  }
  print_summary_table(test, tname, summary);
}

}  // namespace latency

#endif  // EXAMPLES_COMMON_LATENCY_HPP
