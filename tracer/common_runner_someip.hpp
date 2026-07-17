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

#pragma once

// SOME/IP tracer runner, mirrors the embeddedRTPS runner

#include "common_config_someip.hpp"

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/config/communication_config.hpp"
#include "someIp/communication/LinuxUdpTransport.hpp" // host_default_ipv4()
#include "Ipv4Option.hpp"
#include "SdPool.hpp"
#include "LoadBalancingOption.hpp"

#include "soatracer_tp.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>


namespace tracer_someip {

using namespace someIp;
using tracer::AppConfig;
using tracer::ServiceConfig;
using tracer::parse_app_config;

constexpr int EXIT_OK = 0;
constexpr int EXIT_TIMEOUT = 2;
constexpr int EXIT_SEND_FAIL = 3;
constexpr int EXIT_RECV_FAIL = 4;
constexpr int EXIT_CONFIG = 5;

constexpr uint32_t WARMUP_SEQ_PREFIX = 0x80000000u;
constexpr uint32_t WARMUP_SEQ_MASK = 0x7FFFFFFFu;

#ifdef TRACER_KEEP_WARMUP
constexpr std::chrono::seconds WARMUP_DURATION(3);
constexpr std::chrono::milliseconds PARTICIPANT_ENDPOINT_DELAY(1000);
#else
constexpr std::chrono::seconds WARMUP_DURATION(0);
constexpr std::chrono::milliseconds PARTICIPANT_ENDPOINT_DELAY(0);
#endif
constexpr int TIMEOUT_SLACK_MS = 2000;
constexpr int POST_PUBLISH_GRACE_MS = 5000;
// extra time to wait for the SD offer/subscribe handshake
constexpr int SD_DISCOVERY_GRACE_MS = 15000;

// all of a node's topics map onto one fixed service and instance
// each topic gets one event and a packed eventgroup
// mirrors the vsomeip runner so both stacks offer the same structure
// tests run as separate processes, so reusing one service id is safe
constexpr uint16_t SERVICE_BASE = 0x5000;
constexpr uint16_t INSTANCE_ID = 0x0001;
constexpr uint8_t MAJOR_VERSION = 1;
constexpr uint32_t MINOR_VERSION = 0;
constexpr uint16_t EVENT_BASE = 0x8000;
constexpr uint16_t EVENTGROUP_BASE = 0x0001;
constexpr size_t EVENTS_PER_EVENTGROUP = 2; // pack 2 events/eventgroup to fit caps
constexpr size_t MAX_TOPICS_PER_NODE = config::MAX_EVENTS_PER_SERVICE;

inline uint16_t service_id_for(int /*test_id*/) {
  return SERVICE_BASE;
}
inline uint16_t event_id_for(size_t topic_index) {
  return static_cast<uint16_t>(EVENT_BASE + topic_index);
}
inline uint16_t eventgroup_id_for(size_t topic_index) {
  return static_cast<uint16_t>(EVENTGROUP_BASE + (topic_index / EVENTS_PER_EVENTGROUP));
}

// shared helpers for payload protocol, timing and stats
inline const std::chrono::steady_clock::time_point &process_start_time() {
  static const std::chrono::steady_clock::time_point kStart =
      std::chrono::steady_clock::now();
  return kStart;
}

inline uint64_t since_start_ms() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - process_start_time())
          .count());
}

inline bool mark_first(std::atomic<bool> &flag) {
  bool expected = false;
  return flag.compare_exchange_strong(expected, true);
}

inline void emit_diag_event(const char *role, const std::string &topic,
                            const std::string &event,
                            const nlohmann::json &extra = nlohmann::json::object()) {
  nlohmann::json obj;
  obj["t_ms"] = since_start_ms();
  obj["role"] = role;
  obj["topic"] = topic;
  obj["event"] = event;
  if (extra.is_object()) {
    for (auto it = extra.begin(); it != extra.end(); ++it) {
      obj[it.key()] = it.value();
    }
  }
  std::cerr << "TEST_DIAG: " << obj.dump() << std::endl;
}

struct RuntimeOptions {
  int samples = -1;
  std::string config_json;
  int timeout_ms = -1;
  double min_recv_pct = 0.9;
  std::string local_ip;     // --local-ip / SOATRACER_LOCAL_IP / auto-detect
  int base_port = 40000;    // --base-port (local unicast/SD base)
};

struct TraceConfig {
  bool enabled = false;
  int test_id = 0;
};

struct SharedState {
  std::atomic<bool> stop{false};
  std::atomic<bool> failed{false};
  std::atomic<int> fail_code{EXIT_OK};

  std::mutex reason_mutex;
  std::string fail_reason;

  void fail_once(int code, const std::string &reason) {
    bool expected = false;
    if (!failed.compare_exchange_strong(expected, true)) {
      return;
    }
    fail_code.store(code);
    {
      std::lock_guard<std::mutex> lock(reason_mutex);
      fail_reason = reason;
    }
    stop.store(true);
    std::cerr << "TEST_FAIL: " << reason << std::endl;
  }
};

inline bool parse_runtime_options(int argc, char **argv, RuntimeOptions &opts,
                                  std::string &error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if ((arg == "--samples" || arg == "-s") && i + 1 < argc) {
      opts.samples = std::atoi(argv[++i]);
      continue;
    }
    if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
      opts.config_json = argv[++i];
      continue;
    }
    if (arg == "--timeout-ms" && i + 1 < argc) {
      opts.timeout_ms = std::atoi(argv[++i]);
      continue;
    }
    if (arg == "--min-recv-pct" && i + 1 < argc) {
      opts.min_recv_pct = std::atof(argv[++i]);
      continue;
    }
    if (arg == "--local-ip" && i + 1 < argc) {
      opts.local_ip = argv[++i];
      continue;
    }
    if (arg == "--base-port" && i + 1 < argc) {
      opts.base_port = std::atoi(argv[++i]);
      continue;
    }
  }

  if (opts.min_recv_pct < 0.0 || opts.min_recv_pct > 1.0) {
    error = "--min-recv-pct must be in [0.0, 1.0]";
    return false;
  }
  if (opts.samples <= 0) {
    error = "Missing or invalid --samples";
    return false;
  }
  if (opts.config_json.empty()) {
    error = "Missing --config JSON argument";
    return false;
  }
  if (opts.timeout_ms == 0 || opts.timeout_ms < -1) {
    error = "--timeout-ms must be positive when provided";
    return false;
  }
  if (opts.local_ip.empty()) {
    const char *env = std::getenv("SOATRACER_LOCAL_IP");
    if (env != nullptr) {
      opts.local_ip = env;
    }
  }
  return true;
}

inline IpAddr resolve_local_ip(const RuntimeOptions &opts) {
  IpAddr ip;
  if (!opts.local_ip.empty() && IpAddr::from_string(opts.local_ip.c_str(), ip)) {
    return ip;
  }
  return host_default_ipv4();
}

inline bool is_warmup_seq(uint32_t seq) {
  return (seq & WARMUP_SEQ_PREFIX) != 0u;
}

inline void encode_payload(std::vector<uint8_t> &buffer, uint32_t seq) {
  std::memset(buffer.data(), static_cast<int>(seq & 0xFFU), buffer.size());
  std::memcpy(buffer.data(), &seq, sizeof(seq));
}

inline void encode_warmup_payload(std::vector<uint8_t> &buffer, uint32_t warmup_id) {
  const uint32_t warmup_seq = WARMUP_SEQ_PREFIX | (warmup_id & WARMUP_SEQ_MASK);
  std::memset(buffer.data(), static_cast<int>(warmup_seq & 0xFFU), buffer.size());
  std::memcpy(buffer.data(), &warmup_seq, sizeof(warmup_seq));
}

inline bool is_warmup_payload(const uint8_t *data, size_t size) {
  if (size < sizeof(uint32_t)) return false;
  uint32_t seq = 0;
  std::memcpy(&seq, data, sizeof(seq));
  if (!is_warmup_seq(seq)) return false;
  const uint8_t expected = static_cast<uint8_t>(seq & 0xFFU);
  for (size_t i = sizeof(uint32_t); i < size; ++i) {
    if (data[i] != expected) return false;
  }
  return true;
}

inline bool verify_payload(const uint8_t *data, size_t size, uint32_t seq) {
  if (size < sizeof(uint32_t)) return false;
  uint32_t encoded_seq = 0;
  std::memcpy(&encoded_seq, data, sizeof(encoded_seq));
  if (encoded_seq != seq) return false;
  const uint8_t expected = static_cast<uint8_t>(seq & 0xFFU);
  for (size_t i = sizeof(uint32_t); i < size; ++i) {
    if (data[i] != expected) return false;
  }
  return true;
}

inline std::chrono::milliseconds frequency_interval(int hz) {
  const int bounded_hz = std::max(1, hz);
  return std::chrono::milliseconds(std::max(1, 1000 / bounded_hz));
}

// copy the PackageRx payload into out, returns bytes copied
inline size_t read_rx_payload(PackageRx &p, uint8_t *out, size_t cap) {
  const size_t take = std::min(p.payload.size(), cap);
  return p.payload.copy_out(out, take, 0);
}

inline bool read_process_stats_raw(uint64_t &proc_ticks_out,
                                   uint64_t &total_ticks_out,
                                   uint64_t &rss_bytes_out) {
  std::ifstream stat_file("/proc/self/stat");
  if (!stat_file.is_open()) return false;
  std::string stat_line;
  std::getline(stat_file, stat_line);
  stat_file.close();

  std::istringstream stat_iss(stat_line);
  std::string token;
  for (int i = 0; i < 13; ++i) {
    if (!(stat_iss >> token)) return false;
  }
  uint64_t utime = 0, stime = 0;
  if (!(stat_iss >> utime >> stime)) return false;
  proc_ticks_out = utime + stime;

  std::ifstream cpu_file("/proc/stat");
  if (!cpu_file.is_open()) return false;
  std::string cpu_line;
  std::getline(cpu_file, cpu_line);
  cpu_file.close();

  std::istringstream cpu_iss(cpu_line);
  std::string cpu_label;
  cpu_iss >> cpu_label;
  uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0,
           softirq = 0, steal = 0;
  cpu_iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
  total_ticks_out = user + nice + system + idle + iowait + irq + softirq + steal;

  std::ifstream statm_file("/proc/self/statm");
  if (!statm_file.is_open()) return false;
  uint64_t size_pages = 0, rss_pages = 0;
  statm_file >> size_pages >> rss_pages;
  statm_file.close();

  long page_size = ::sysconf(_SC_PAGESIZE);
  if (page_size <= 0) page_size = 4096;
  rss_bytes_out = rss_pages * static_cast<uint64_t>(page_size);
  return true;
}

inline void emit_experiment_begin(const TraceConfig &trace_cfg) {
  if (!trace_cfg.enabled) return;
  lttng_ust_tracepoint(soatracer_trace_provider, experiment_begin,
                       static_cast<uint32_t>(trace_cfg.test_id),
                       static_cast<uint64_t>(trace_cfg.test_id));
}

inline void emit_experiment_end(const TraceConfig &trace_cfg) {
  if (!trace_cfg.enabled) return;
  lttng_ust_tracepoint(soatracer_trace_provider, experiment_end,
                       static_cast<uint32_t>(trace_cfg.test_id),
                       static_cast<uint64_t>(trace_cfg.test_id));
}

inline sd::SdTransportProtocol sd_proto_for(TransportKind transport) {
  return transport == TransportKind::TCP ? sd::SdTransportProtocol::TCP
                                         : sd::SdTransportProtocol::UDP;
}

// publisher
struct PublisherWorker {
  ServiceConfig cfg;
  int samples = 0;
  TransportKind transport = TransportKind::UDP;
  TraceConfig trace_cfg;
  IpAddr local_ip;
  int base_port = 40000;
  std::shared_ptr<SharedState> state;
  std::atomic<bool> done{false};
  std::atomic<bool> first_warmup_sent_logged{false};
  std::atomic<bool> first_regular_sent_logged{false};

  std::vector<int> topic_timing_offsets_us;
  std::vector<uint32_t> sent_counts;
  std::vector<uint32_t> last_seq_per_topic;

  void send_notification(ESomeIp &api, const someIp::PoolPtr<sd::SdService> &svc,
                         uint16_t service_id, size_t topic_index,
                         const std::vector<uint8_t> &payload) {
    const uint16_t event_id = event_id_for(topic_index);
    const uint16_t eg_id = eventgroup_id_for(topic_index);
    sd::SubscriberVec subs = svc->get_subscribers_of_eventgroup(eg_id);
    for (auto subscriber : subs) {
      const auto &ep = subscriber._option;
      Package pkg(service_id, event_id, payload.data(), payload.size(), ep._ip, ep._port);
      pkg.header._message_type = MessageType::NOTIFICATION;
      api.fire_and_forget(std::move(pkg), transport);
    }
  }

  size_t subscriber_count(const someIp::PoolPtr<sd::SdService> &svc,
                          const std::set<uint16_t> &eventgroups) {
    size_t n = 0;
    for (uint16_t eg : eventgroups) {
      n += svc->get_subscribers_of_eventgroup(eg).size();
    }
    return n;
  }

  void operator()() {
    const std::string &diag_label = cfg.topics.front();
    const size_t num_topics = cfg.topics.size();

    ESomeIp api;
    api.init(local_ip);
    api.set_transport_mode(transport);
    const uint16_t actual_port = api.listen_to_port(base_port);

    const uint16_t service_id = service_id_for(trace_cfg.test_id);
    sd::SdServiceInfo info{service_id, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION};
    info._src_port = actual_port;
    info._weight = 10;
    info._priority = 5;
    auto sd_service = sd::pool_make_service<sd::SdService>(info);

    std::set<uint16_t> eventgroups;
    for (size_t t = 0; t < num_topics; ++t) {
      sd_service->register_event(event_id_for(t), eventgroup_id_for(t), false, transport);
      eventgroups.insert(eventgroup_id_for(t));
    }

    sd::ServiceVec sd_services;
    api.enable_sd(local_ip, sd_services);
    api.register_sd_service(sd_service);

    sd::OptionVec options;
    options.push_back(sd::pool_make<sd::LoadBalancingOption>(info._weight, info._priority, true));
    options.push_back(sd::pool_make<sd::Ipv4Option>(local_ip, actual_port, sd_proto_for(transport)));
    sd_service->update_options(options);
    api.offer_service(sd_service, local_ip, actual_port);

    emit_diag_event("publisher", diag_label, "service_offered", {
        {"topics", num_topics},
        {"service_id", service_id},
        {"port", actual_port},
    });

    // gate on at least one subscriber per eventgroup
    const auto sd_deadline = std::chrono::steady_clock::now() +
                             std::chrono::milliseconds(SD_DISCOVERY_GRACE_MS);
    while (!state->stop.load() && std::chrono::steady_clock::now() < sd_deadline) {
      bool all_ready = true;
      for (uint16_t eg : eventgroups) {
        if (sd_service->get_subscribers_of_eventgroup(eg).empty()) {
          all_ready = false;
          break;
        }
      }
      if (all_ready) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    emit_diag_event("publisher", diag_label, "subscriptions_ready", {
        {"subscribers", subscriber_count(sd_service, eventgroups)},
    });

    const auto publisher_start = std::chrono::steady_clock::now();
    std::vector<bool> first_publish_logged(num_topics, false);
    sent_counts.assign(num_topics, 0);
    last_seq_per_topic.assign(num_topics, 0);

    std::vector<uint8_t> payload(static_cast<size_t>(cfg.payload_size), 0);

    const std::chrono::milliseconds interval = frequency_interval(cfg.frequency_hz);
    const std::chrono::microseconds offset(cfg.publish_offset_us);
    const std::chrono::microseconds timing_offset(cfg.timing_offset_us);
    const bool has_per_topic_offsets = !topic_timing_offsets_us.empty() &&
        topic_timing_offsets_us.size() == num_topics;
    std::chrono::steady_clock::time_point next_deadline =
        std::chrono::steady_clock::now() + interval;

    // warmup
    const std::chrono::steady_clock::time_point warmup_end =
        std::chrono::steady_clock::now() + WARMUP_DURATION;
    uint32_t warmup_id = 0;
    while (!state->stop.load() && std::chrono::steady_clock::now() < warmup_end) {
      if (timing_offset.count() > 0) std::this_thread::sleep_for(timing_offset);
      const auto period_start_wu = std::chrono::steady_clock::now();
      for (size_t t = 0; t < num_topics; ++t) {
        if (has_per_topic_offsets && topic_timing_offsets_us[t] > 0) {
          std::this_thread::sleep_until(
              period_start_wu + std::chrono::microseconds(topic_timing_offsets_us[t]));
        }
        const uint32_t current_warmup_id = warmup_id++;
        encode_warmup_payload(payload, current_warmup_id);
        if (current_warmup_id == 0 && mark_first(first_warmup_sent_logged)) {
          emit_diag_event("publisher", cfg.topics[t], "first_warmup_sent", {{"warmup_id", 0}});
        }
        send_notification(api, sd_service, service_id, t, payload);
        if (!has_per_topic_offsets && offset.count() > 0 && t + 1 < num_topics) {
          std::this_thread::sleep_for(offset);
        }
      }
      std::this_thread::sleep_until(next_deadline);
      next_deadline += interval;
    }

    uint32_t seq = 0;
    for (int i = 0; i < samples; ++i) {
      if (state->stop.load()) {
        done.store(true);
        return;
      }
      if (timing_offset.count() > 0) std::this_thread::sleep_for(timing_offset);

      seq = static_cast<uint32_t>(i);
      encode_payload(payload, seq);

      const auto period_start = std::chrono::steady_clock::now();
      for (size_t t = 0; t < num_topics; ++t) {
        if (has_per_topic_offsets && topic_timing_offsets_us[t] > 0) {
          std::this_thread::sleep_until(
              period_start + std::chrono::microseconds(topic_timing_offsets_us[t]));
        }

        if (seq == 0 && t == 0 && mark_first(first_regular_sent_logged)) {
          emit_diag_event("publisher", cfg.topics[t], "first_regular_sent", {{"seq", 0}});
        }

        if (!first_publish_logged[t]) {
          first_publish_logged[t] = true;
          const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::steady_clock::now() - publisher_start).count();
          if (trace_cfg.enabled) {
            lttng_ust_tracepoint(soatracer_trace_provider, first_publish,
                                 cfg.topics[t].c_str(), cfg.idx, trace_cfg.test_id,
                                 static_cast<uint64_t>(elapsed));
          }
        }

        if (trace_cfg.enabled) {
          lttng_ust_tracepoint(soatracer_trace_provider, pre_publish,
                               cfg.topics[t].c_str(), cfg.idx, cfg.type_index,
                               static_cast<int>(seq), trace_cfg.test_id);
        }

        send_notification(api, sd_service, service_id, t, payload);

        if (trace_cfg.enabled) {
          lttng_ust_tracepoint(soatracer_trace_provider, after_publish,
                               cfg.topics[t].c_str(), cfg.idx, cfg.type_index,
                               static_cast<int>(seq), trace_cfg.test_id);
        }

        sent_counts[t] += 1;
        last_seq_per_topic[t] = seq;

        if (!has_per_topic_offsets && offset.count() > 0 && t + 1 < num_topics) {
          std::this_thread::sleep_for(offset);
        }
      }

      if (cfg.sending_pattern == "sporadic") {
        static thread_local std::mt19937 sporadic_rng{std::random_device{}()};
        const auto min_ms = interval.count() / 4;
        const auto max_ms = interval.count() * 5 / 2;
        std::uniform_int_distribution<long> dist(std::max(1L, min_ms), std::max(2L, max_ms));
        std::this_thread::sleep_for(std::chrono::milliseconds(dist(sporadic_rng)));
        next_deadline = std::chrono::steady_clock::now() + interval;
      } else {
        std::this_thread::sleep_until(next_deadline);
        next_deadline += interval;
      }
    }

    if (trace_cfg.enabled) {
      for (size_t t = 0; t < num_topics; ++t) {
        lttng_ust_tracepoint(soatracer_trace_provider, published_all,
                             cfg.topics[t].c_str(), cfg.idx, cfg.type_index,
                             static_cast<int>(seq), trace_cfg.test_id);
      }
    }

    {
      const auto grace_end = std::chrono::steady_clock::now() +
          std::chrono::milliseconds(POST_PUBLISH_GRACE_MS);
      while (!state->stop.load() && std::chrono::steady_clock::now() < grace_end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    api.stop_all_services();
    done.store(true);
  }
};

// subscriber
struct TopicRecvCtx {
  std::string topic;
  int payload_size = 0;
  int idx = 0;
  int type_index = 0;
  TraceConfig trace_cfg;
  int target_samples = 0;
  std::shared_ptr<SharedState> shared_state;
  std::condition_variable *completion_cv = nullptr;
  std::chrono::steady_clock::time_point subscriber_start;

  std::mutex mutex;
  uint32_t expected_seq = 0;
  uint32_t received = 0;
  uint32_t initial_loss = 0;
  uint32_t mid_stream_loss = 0;
  std::atomic<bool> topic_done{false};
  std::atomic<bool> first_warmup_received_logged{false};
  std::atomic<bool> first_regular_received_logged{false};

  void handle(PackageRx &&p) {
    if (shared_state->stop.load()) return;

    std::vector<uint8_t> buffer(static_cast<size_t>(payload_size), 0);
    const size_t got = read_rx_payload(p, buffer.data(), buffer.size());
    if (got != static_cast<size_t>(payload_size)) {
      // ignore stray/short notification
      emit_diag_event("subscriber", topic, "size_mismatch", {
          {"expected", payload_size}, {"got", got}});
      return;
    }

    uint32_t seq = 0;
    std::memcpy(&seq, buffer.data(), sizeof(seq));

    if (is_warmup_payload(buffer.data(), buffer.size())) {
      if (mark_first(first_warmup_received_logged)) {
        emit_diag_event("subscriber", topic, "first_warmup_received",
                        {{"warmup_id", seq & WARMUP_SEQ_MASK}});
      }
      return;
    }
    if (mark_first(first_regular_received_logged)) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - subscriber_start).count();
      emit_diag_event("subscriber", topic, "first_regular_received", {
          {"seq", seq}, {"elapsed_ms", static_cast<int64_t>(elapsed / 1000000)}});
      if (trace_cfg.enabled) {
        lttng_ust_tracepoint(soatracer_trace_provider, first_receive,
                             topic.c_str(), idx, trace_cfg.test_id,
                             static_cast<uint64_t>(elapsed), static_cast<int>(seq));
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex);

      if (seq < expected_seq) {
        emit_diag_event("subscriber", topic, "out_of_order", {{"seq", seq}, {"expected", expected_seq}});
        return;
      }
      if (seq > expected_seq) {
        uint32_t gap = seq - expected_seq;
        if (received == 0) {
          initial_loss = gap;
          emit_diag_event("subscriber", topic, "initial_loss", {{"skipped", gap}, {"first_seq", seq}});
        } else {
          mid_stream_loss += gap;
          emit_diag_event("subscriber", topic, "mid_stream_loss",
                          {{"skipped", gap}, {"expected", expected_seq}, {"got", seq}});
        }
        expected_seq = seq;
      }

      if (!verify_payload(buffer.data(), buffer.size(), seq)) {
        shared_state->fail_once(EXIT_RECV_FAIL,
                                "payload mismatch topic=" + topic + " seq=" + std::to_string(seq));
        completion_cv->notify_all();
        return;
      }

      ++expected_seq;
      ++received;

      if (trace_cfg.enabled) {
        lttng_ust_tracepoint(soatracer_trace_provider, after_receiving,
                             topic.c_str(), idx, static_cast<int>(received), type_index,
                             0, static_cast<int>(seq), trace_cfg.test_id);
      }

      emit_diag_event("subscriber", topic, "received", {{"count", received}, {"seq", seq}});

      const uint32_t target = static_cast<uint32_t>(target_samples);
      const uint32_t total_observed = received + initial_loss + mid_stream_loss;
      const bool saw_last_seq = (target > 0) && (seq + 1 >= target);
      if (saw_last_seq || total_observed >= target) {
        if (trace_cfg.enabled) {
          lttng_ust_tracepoint(soatracer_trace_provider, received_all,
                               topic.c_str(), idx, static_cast<int>(received), type_index,
                               static_cast<int>(seq), trace_cfg.test_id);
        }
        emit_diag_event("subscriber", topic, "done", {
            {"received", received}, {"initial_loss", initial_loss},
            {"mid_stream_loss", mid_stream_loss}, {"last_seq", seq}});
        topic_done.store(true);
      }
    }

    completion_cv->notify_all();
  }
};

struct SubscriberWorker {
  ServiceConfig cfg;
  int samples = 0;
  TransportKind transport = TransportKind::UDP;
  TraceConfig trace_cfg;
  IpAddr local_ip;
  int base_port = 40000;
  std::shared_ptr<SharedState> state;
  std::atomic<bool> done{false};

  std::mutex wait_mutex;
  std::condition_variable cv;
  std::vector<std::unique_ptr<TopicRecvCtx>> topic_ctxs;

  void operator()() {
    const std::string &diag_label = cfg.topics.front();
    const size_t num_topics = cfg.topics.size();

    ESomeIp api;
    api.init(local_ip);
    api.set_transport_mode(transport);

    const uint16_t service_id = service_id_for(trace_cfg.test_id);

    const auto subscriber_start = std::chrono::steady_clock::now();
    topic_ctxs.reserve(num_topics);
    for (size_t t = 0; t < num_topics; ++t) {
      std::unique_ptr<TopicRecvCtx> ctx(new TopicRecvCtx());
      ctx->topic = cfg.topics[t];
      ctx->payload_size = cfg.payload_size;
      ctx->idx = cfg.idx;
      ctx->type_index = cfg.type_index;
      ctx->trace_cfg = trace_cfg;
      ctx->target_samples = samples;
      ctx->shared_state = state;
      ctx->completion_cv = &cv;
      ctx->subscriber_start = subscriber_start;
      TopicRecvCtx *raw = ctx.get();
      api.register_event_handler(service_id, event_id_for(t),
                                 [raw](PackageRx &&p) { raw->handle(std::move(p)); });
      emit_diag_event("subscriber", cfg.topics[t], "handler_registered", {{"idx", t}});
      topic_ctxs.push_back(std::move(ctx));
    }

    // SD rides UDP, notifications arrive on the advertised endpoint
    const uint16_t udp_port = api.listen_to_port(base_port);
    uint16_t advertise_port = udp_port;
    if (transport == TransportKind::TCP) {
      api.open_tcp_listener(static_cast<uint16_t>(base_port));
      advertise_port = static_cast<uint16_t>(base_port);
    }

    sd::ServiceVec sd_services;
    api.enable_sd(local_ip, sd_services);

    sd::SdServiceInfo info{service_id, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION};
    auto client = sd::pool_make<sd::SdClientService>(info);
    api.register_client_service(client);

    const auto discover_deadline = std::chrono::steady_clock::now() +
                                   std::chrono::milliseconds(SD_DISCOVERY_GRACE_MS);
    while (client->get_phase() != sd::SdClientPhase::MAIN &&
           client->get_phase() != sd::SdClientPhase::STOPPED &&
           !state->stop.load() &&
           std::chrono::steady_clock::now() < discover_deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    emit_diag_event("subscriber", diag_label, "discovered",
                    {{"phase_main", client->get_phase() == sd::SdClientPhase::MAIN}});

    // re-send SUBSCRIBE periodically since the SD layer sends it only once
    std::set<uint16_t> eventgroups;
    for (size_t t = 0; t < num_topics; ++t) eventgroups.insert(eventgroup_id_for(t));
    auto do_subscribe = [&]() {
      for (uint16_t eg : eventgroups) {
        sd::OptionVec options;
        options.push_back(sd::pool_make<sd::Ipv4Option>(
            local_ip, advertise_port, sd_proto_for(transport), false));
        api.subscribe(sd::SdEventgroupInfo{service_id, INSTANCE_ID, MAJOR_VERSION, eg}, options);
      }
    };
    auto any_received = [&]() {
      for (const auto &ctx : topic_ctxs) if (ctx->received > 0) return true;
      return false;
    };
    do_subscribe();
    emit_diag_event("subscriber", diag_label, "subscribed", {{"eventgroups", eventgroups.size()}});

    std::unique_lock<std::mutex> lock(wait_mutex);
    auto last_resubscribe = std::chrono::steady_clock::now();
    while (!state->stop.load()) {
      bool all_topics_done = true;
      for (const auto &ctx : topic_ctxs) {
        if (!ctx->topic_done.load()) { all_topics_done = false; break; }
      }
      if (all_topics_done) {
        api.unsubscribe_all();
        done.store(true);
        return;
      }
      // resend a lost SUBSCRIBE at most once a second until the first notification
      if (!any_received() &&
          std::chrono::steady_clock::now() - last_resubscribe >= std::chrono::milliseconds(1000)) {
        do_subscribe();
        last_resubscribe = std::chrono::steady_clock::now();
        emit_diag_event("subscriber", diag_label, "resubscribe", {});
      }
      cv.wait_for(lock, std::chrono::milliseconds(50));
    }

    bool all_topics_done = true;
    for (const auto &ctx : topic_ctxs) {
      if (!ctx->topic_done.load()) { all_topics_done = false; break; }
    }
    api.unsubscribe_all();
    done.store(all_topics_done);
  }
};

// entry point
inline int run_mode_someip(int argc, char **argv, TransportKind transport,
                           bool emit_tracepoints) {
  RuntimeOptions options;
  std::string error;
  if (!parse_runtime_options(argc, argv, options, error)) {
    std::cerr << "Config/arg error: " << error << std::endl;
    return EXIT_CONFIG;
  }

  AppConfig app_config;
  if (!parse_app_config(options.config_json, app_config, error)) {
    std::cerr << "Config/arg error: " << error << std::endl;
    return EXIT_CONFIG;
  }

  const int max_hz = app_config.max_frequency_hz();
  const bool has_sporadic = [&]() {
    for (const auto &svc : app_config.services)
      if (svc.sending_pattern == "sporadic") return true;
    return false;
  }();
  const int timing_multiplier = has_sporadic ? 3 : 1;
  const int default_timeout_ms =
      std::max(15000, timing_multiplier * ((options.samples * 1000) / std::max(1, max_hz)) +
                          10000 + TIMEOUT_SLACK_MS + POST_PUBLISH_GRACE_MS + SD_DISCOVERY_GRACE_MS +
                          static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               WARMUP_DURATION).count()));
  const int timeout_ms = options.timeout_ms > 0 ? options.timeout_ms : default_timeout_ms;

  const IpAddr local_ip = resolve_local_ip(options);

  std::shared_ptr<SharedState> state(new SharedState());

  TraceConfig trace_cfg;
  trace_cfg.enabled = emit_tracepoints;
  if (app_config.test_id > 0) {
    trace_cfg.test_id = app_config.test_id;
  } else {
    trace_cfg.test_id = app_config.services.front().idx + 1;
  }

  // merge pub/sub services and enforce the SOME/IP cap
  ServiceConfig merged_pub, merged_sub;
  bool has_pub = false, has_sub = false;
  std::vector<int> pub_timing_offsets;
  for (const ServiceConfig &svc : app_config.services) {
    if (svc.type == "pub") {
      if (!has_pub) { merged_pub = svc; has_pub = true; }
      else { for (const auto &t : svc.topics) merged_pub.topics.push_back(t); }
      for (size_t i = 0; i < svc.topics.size(); ++i) pub_timing_offsets.push_back(svc.timing_offset_us);
    } else if (svc.type == "sub") {
      if (!has_sub) { merged_sub = svc; has_sub = true; }
      else { for (const auto &t : svc.topics) merged_sub.topics.push_back(t); }
    }
  }

  if ((has_pub && merged_pub.topics.size() > MAX_TOPICS_PER_NODE) ||
      (has_sub && merged_sub.topics.size() > MAX_TOPICS_PER_NODE)) {
    std::cerr << "Config/arg error: embeddedSOMEIP node supports at most "
              << MAX_TOPICS_PER_NODE << " topics per node (StackConfig caps)" << std::endl;
    return EXIT_CONFIG;
  }

  if (!pub_timing_offsets.empty()) {
    int min_offset = *std::min_element(pub_timing_offsets.begin(), pub_timing_offsets.end());
    merged_pub.timing_offset_us = min_offset;
    for (auto &v : pub_timing_offsets) v -= min_offset;
  }

  emit_experiment_begin(trace_cfg);

  std::thread stats_thread;
  if (trace_cfg.enabled) {
    stats_thread = std::thread([state]() {
      uint64_t prev_proc_ticks = 0, prev_total_ticks = 0, prev_rss_bytes = 0;
      if (!read_process_stats_raw(prev_proc_ticks, prev_total_ticks, prev_rss_bytes)) return;
      const unsigned int hw_threads = std::thread::hardware_concurrency();
      const double num_cpus = hw_threads > 0 ? static_cast<double>(hw_threads) : 1.0;
      while (!state->stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        uint64_t proc_ticks = 0, total_ticks = 0, rss_bytes = 0;
        if (!read_process_stats_raw(proc_ticks, total_ticks, rss_bytes)) continue;
        const uint64_t delta_proc = proc_ticks - prev_proc_ticks;
        const uint64_t delta_total = total_ticks - prev_total_ticks;
        double cpu_percent = 0.0;
        if (delta_total > 0) {
          cpu_percent = (static_cast<double>(delta_proc) / static_cast<double>(delta_total)) *
                        num_cpus * 100.0;
        }
        const double mem_mb = static_cast<double>(rss_bytes) / (1024.0 * 1024.0);
        lttng_ust_tracepoint(soatracer_trace_provider, machine_stats, cpu_percent, mem_mb);
        prev_proc_ticks = proc_ticks;
        prev_total_ticks = total_ticks;
      }
    });
  }

  const std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  std::vector<std::unique_ptr<PublisherWorker>> pubs;
  std::vector<std::unique_ptr<SubscriberWorker>> subs;
  std::vector<std::thread> workers;

  if (has_pub) {
    std::unique_ptr<PublisherWorker> worker(new PublisherWorker());
    worker->cfg = merged_pub;
    worker->topic_timing_offsets_us = std::move(pub_timing_offsets);
    worker->samples = options.samples;
    worker->transport = transport;
    worker->trace_cfg = trace_cfg;
    worker->local_ip = local_ip;
    worker->base_port = options.base_port;
    worker->state = state;
    workers.push_back(std::thread(std::ref(*worker)));
    pubs.push_back(std::move(worker));
  }

  if (has_sub) {
    std::unique_ptr<SubscriberWorker> worker(new SubscriberWorker());
    worker->cfg = merged_sub;
    worker->samples = options.samples;
    worker->transport = transport;
    worker->trace_cfg = trace_cfg;
    worker->local_ip = local_ip;
    // offset the sub so a colocated pub does not collide on base_port
    worker->base_port = has_pub ? options.base_port + 1 : options.base_port;
    worker->state = state;
    workers.push_back(std::thread(std::ref(*worker)));
    subs.push_back(std::move(worker));
  }

  while (!state->stop.load()) {
    bool all_done = true;
    for (const auto &p : pubs) if (!p->done.load()) { all_done = false; break; }
    if (all_done) for (const auto &s : subs) if (!s->done.load()) { all_done = false; break; }
    if (all_done) break;

    if (std::chrono::steady_clock::now() > deadline) {
      for (const auto &pub : pubs) {
        for (size_t t = 0; t < pub->cfg.topics.size(); ++t) {
          uint32_t sent = t < pub->sent_counts.size() ? pub->sent_counts[t] : 0;
          uint32_t last_seq = t < pub->last_seq_per_topic.size() ? pub->last_seq_per_topic[t] : 0;
          emit_diag_event("publisher", pub->cfg.topics[t], "timeout_progress",
                          {{"sent", sent}, {"expected", pub->samples}, {"last_seq", last_seq}});
        }
      }
      bool worst_zero = false, worst_below_threshold = false;
      for (const auto &sub : subs) {
        for (const auto &ctx : sub->topic_ctxs) {
          const uint32_t received = ctx->received;
          const uint32_t expected = static_cast<uint32_t>(ctx->target_samples);
          const double pct = expected > 0 ? static_cast<double>(received) / expected : 1.0;
          if (received == 0 && expected > 0) worst_zero = true;
          else if (pct < options.min_recv_pct) worst_below_threshold = true;
          emit_diag_event("subscriber", ctx->topic, "timeout_progress",
                          {{"received", received}, {"expected", expected},
                           {"done", ctx->topic_done.load()}, {"recv_pct", pct}});
        }
      }
      if (worst_zero) {
        state->fail_once(EXIT_TIMEOUT,
                         "global timeout exceeded with zero receives on at least one topic (" +
                             std::to_string(timeout_ms) + " ms)");
      } else if (worst_below_threshold) {
        state->fail_once(EXIT_TIMEOUT,
                         "global timeout exceeded; receive rate below min_recv_pct=" +
                             std::to_string(options.min_recv_pct) + " (" +
                             std::to_string(timeout_ms) + " ms)");
      } else {
        emit_diag_event("publisher", "", "timeout_within_tolerance", {{"min_recv_pct", options.min_recv_pct}});
        for (auto &s : subs) {
          s->done.store(true);
          for (auto &ctx : s->topic_ctxs) ctx->topic_done.store(true);
        }
      }
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  state->stop.store(true);
  for (const auto &s : subs) s->cv.notify_all();
  for (auto &w : workers) if (w.joinable()) w.join();
  if (stats_thread.joinable()) stats_thread.join();

  emit_experiment_end(trace_cfg);

  if (state->failed.load()) {
    return state->fail_code.load();
  }
  for (const auto &p : pubs) {
    if (!p->done.load()) {
      std::cerr << "TEST_FAIL: publisher did not complete" << std::endl;
      return EXIT_SEND_FAIL;
    }
  }
  for (const auto &s : subs) {
    if (!s->done.load()) {
      std::cerr << "TEST_FAIL: subscriber did not complete" << std::endl;
      return EXIT_RECV_FAIL;
    }
  }

  std::cout << "TEST_PASS" << std::endl;
  return EXIT_OK;
}

} // namespace tracer_someip
