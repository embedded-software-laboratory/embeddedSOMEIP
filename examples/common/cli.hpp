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

#ifndef EXAMPLES_COMMON_CLI_HPP
#define EXAMPLES_COMMON_CLI_HPP

#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <vector>

#include "someIp/ESomeIp.hpp"
#include "common/constants.hpp"

namespace example {

struct Config {
  std::string client_ip = example::CLIENT_IP;
  std::string server_ip = example::SERVER_IP;
  uint16_t client_port = example::CLIENT_PORT;
  uint16_t server_port = example::SERVER_PORT;
  uint16_t service_id = example::SERVICE_ID;
  uint16_t instance_id = example::INSTANCE_ID;
  uint16_t major = example::MAJOR_VERSION;
  uint16_t minor = example::MINOR_VERSION;
  uint16_t method_id = example::RPC_METHOD_ID;
  uint16_t shutdown_method_id = example::SHUTDOWN_METHOD_ID;
  uint16_t event_id = example::EVENT_ID;
  uint16_t eventgroup_id = example::EVENTGROUP_ID;
  size_t payload_size = example::UDP_PAYLOAD_SIZE;
  int num_messages = 2;
  int notify_count = 5;
  int notify_interval_ms = 500;
  // samples per payload size in the latency sweeps
  int num_samples = 10000;
  bool samples_set = false;  // unset lets each test pick its own default
  std::string csv_path;
  bool use_sd = true;
  bool show_help = false;
  someIp::TransportKind transport = someIp::TransportKind::UDP;
  // T2 active multicasts a FindService, passive waits for a cyclic OfferService
  std::string sd_mode = "active";
  // pause between samples that tear state down (T2 recreate, T4 re-subscribe)
  int settle_ms = 200;
  // payload-size list override for T5 (comma-separated bytes)
  std::string sizes;
};

// comma-separated size list ("2048,4096"), empty/invalid entries are skipped
inline std::vector<size_t> parse_size_list(const std::string &value) {
  std::vector<size_t> out;
  size_t pos = 0;
  while (pos <= value.size()) {
    size_t comma = value.find(',', pos);
    if (comma == std::string::npos) comma = value.size();
    std::string tok = value.substr(pos, comma - pos);
    if (!tok.empty()) {
      char *end = nullptr;
      unsigned long v = std::strtoul(tok.c_str(), &end, 10);
      if (end != tok.c_str() && *end == '\0' && v > 0) out.push_back(static_cast<size_t>(v));
    }
    pos = comma + 1;
  }
  return out;
}

inline bool parse_bool(const std::string &value, bool &out) {
  if (value == "1" || value == "true" || value == "yes" || value == "on") {
    out = true;
    return true;
  }
  if (value == "0" || value == "false" || value == "no" || value == "off") {
    out = false;
    return true;
  }
  return false;
}

inline bool parse_transport(const std::string &value, someIp::TransportKind &out) {
  if (value == "udp") {
    out = someIp::TransportKind::UDP;
    return true;
  }
  if (value == "tp") {
    out = someIp::TransportKind::UDP_TP;
    return true;
  }
  if (value == "tcp") {
    out = someIp::TransportKind::TCP;
    return true;
  }
  return false;
}

inline uint16_t parse_u16(const std::string &value, uint16_t fallback) {
  char *end = nullptr;
  unsigned long v = std::strtoul(value.c_str(), &end, 10);
  // end == value.c_str() means no conversion happened
  if (end == value.c_str() || *end != '\0' || v > 0xFFFF) return fallback;
  return static_cast<uint16_t>(v);
}

inline int parse_i32(const std::string &value, int fallback) {
  char *end = nullptr;
  long v = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') return fallback;
  return static_cast<int>(v);
}

inline size_t parse_size(const std::string &value, size_t fallback) {
  char *end = nullptr;
  unsigned long v = std::strtoul(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') return fallback;
  return static_cast<size_t>(v);
}

inline void apply_arg(Config &cfg, const std::string &arg) {
  if (arg == "--help" || arg == "-h") {
    cfg.show_help = true;
    return;
  }
  if (arg == "--sd" || arg == "-s") {
    cfg.use_sd = true;
    return;
  }
  if (arg == "--no-sd") {
    cfg.use_sd = false;
    return;
  }
  if (arg.rfind("--", 0) != 0) return;
  auto eq = arg.find('=');
  std::string key = (eq == std::string::npos) ? arg.substr(2) : arg.substr(2, eq - 2);
  std::string value = (eq == std::string::npos) ? "" : arg.substr(eq + 1);

  if (key == "client-ip") cfg.client_ip = value;
  else if (key == "server-ip") cfg.server_ip = value;
  else if (key == "client-port") cfg.client_port = parse_u16(value, cfg.client_port);
  else if (key == "server-port") cfg.server_port = parse_u16(value, cfg.server_port);
  else if (key == "service-id") cfg.service_id = parse_u16(value, cfg.service_id);
  else if (key == "instance-id") cfg.instance_id = parse_u16(value, cfg.instance_id);
  else if (key == "major") cfg.major = parse_u16(value, cfg.major);
  else if (key == "minor") cfg.minor = parse_u16(value, cfg.minor);
  else if (key == "method-id") cfg.method_id = parse_u16(value, cfg.method_id);
  else if (key == "shutdown-id") cfg.shutdown_method_id = parse_u16(value, cfg.shutdown_method_id);
  else if (key == "event-id") cfg.event_id = parse_u16(value, cfg.event_id);
  else if (key == "eventgroup-id") cfg.eventgroup_id = parse_u16(value, cfg.eventgroup_id);
  else if (key == "payload-size") cfg.payload_size = parse_size(value, cfg.payload_size);
  else if (key == "num-messages") cfg.num_messages = parse_i32(value, cfg.num_messages);
  else if (key == "samples") {
    cfg.num_samples = parse_i32(value, cfg.num_samples);
    cfg.samples_set = true;
  }
  else if (key == "csv") cfg.csv_path = value;
  else if (key == "sd-mode") {
    if (value == "active" || value == "passive") cfg.sd_mode = value;
    else {
      std::fprintf(stderr, "Invalid value for --sd-mode: '%s' (expected active|passive)\n", value.c_str());
      cfg.show_help = true;
    }
  }
  else if (key == "settle-ms") cfg.settle_ms = parse_i32(value, cfg.settle_ms);
  else if (key == "sizes") cfg.sizes = value;
  else if (key == "notify-count") cfg.notify_count = parse_i32(value, cfg.notify_count);
  else if (key == "notify-interval-ms") cfg.notify_interval_ms = parse_i32(value, cfg.notify_interval_ms);
  else if (key == "use-sd") {
    if (value.empty() || !parse_bool(value, cfg.use_sd)) {
      std::fprintf(stderr, "Invalid value for --use-sd: '%s' (expected 0/1/true/false)\n", value.c_str());
      cfg.show_help = true;
    }
  }
  else if (key == "transport") {
    if (!parse_transport(value, cfg.transport)) {
      std::fprintf(stderr, "Invalid value for --transport: '%s' (expected udp|tp|tcp)\n", value.c_str());
      cfg.show_help = true;
    }
  }
}

inline bool parse_transport_short(const std::string &value, someIp::TransportKind &out) {
  if (value == "udp") {
    out = someIp::TransportKind::UDP;
    return true;
  }
  if (value == "tp") {
    out = someIp::TransportKind::UDP_TP;
    return true;
  }
  if (value == "tcp") {
    out = someIp::TransportKind::TCP;
    return true;
  }
  return parse_transport(value, out);
}

inline void parse_args(int argc, char *argv[], Config &cfg) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-t" && i + 1 < argc) {
      parse_transport_short(argv[++i], cfg.transport);
      continue;
    }
    // combined form like -tudp or -ttp
    if (arg.rfind("-t", 0) == 0 && arg.size() > 2) {
      parse_transport_short(arg.substr(2), cfg.transport);
      continue;
    }
    apply_arg(cfg, arg);
  }
}

inline void print_common_usage(const char *name) {
  printf("Usage: %s [-s|--sd|--no-sd] [-t udp|tp|tcp] [--use-sd=0|1] [--transport=udp|tp|tcp] [--client-ip=IP] [--server-ip=IP]\\n", name);
  printf("            [--client-port=PORT] [--server-port=PORT] [--service-id=ID] [--instance-id=ID]\\n");
  printf("            [--method-id=ID] [--event-id=ID] [--eventgroup-id=ID] [--payload-size=N]\\n");
  printf("            [--num-messages=N] [--notify-count=N] [--notify-interval-ms=MS]\\n");
}

} // namespace example

#endif // EXAMPLES_COMMON_CLI_HPP
