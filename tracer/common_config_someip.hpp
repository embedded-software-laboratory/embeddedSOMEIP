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

// trimmed copy of embeddedrtps tracer common_config.hpp without the FeatureQOS include

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace tracer {

using json = nlohmann::json;

struct ServiceConfig {
  std::string name;
  std::string type;
  int idx = 0;
  int type_index = 0;
  std::string topic;
  std::vector<std::string> topics;
  int frequency_hz = 10;
  int payload_size = 16;
  int aggregation_deadline_ms = 100;
  int publish_offset_us = 0;
  int timing_offset_us = 0;
  bool reliable = false;
  std::string sending_pattern = "periodic"; // "periodic", "sporadic"
};

struct AppConfig {
  int test_id = -1;
  std::vector<ServiceConfig> services;

  int max_frequency_hz() const {
    int max_hz = 1;
    for (std::vector<ServiceConfig>::const_iterator it = services.begin(); it != services.end(); ++it) {
      if (it->frequency_hz > max_hz) {
        max_hz = it->frequency_hz;
      }
    }
    return max_hz;
  }
};

inline bool parse_string_param(const json &node, const char *key, std::string &out) {
  if (!node.is_object() || !node.contains(key)) return false;
  const json &value = node.at(key);
  if (value.is_string()) { out = value.get<std::string>(); return true; }
  return false;
}

inline bool parse_int_param(const json &node, const char *key, int &out) {
  if (!node.is_object() || !node.contains(key)) {
    return false;
  }
  const json &value = node.at(key);
  try {
    if (value.is_number_integer()) {
      out = value.get<int>();
      return true;
    }
    if (value.is_number_float()) {
      out = static_cast<int>(value.get<double>());
      return true;
    }
    if (value.is_string()) {
      out = std::stoi(value.get<std::string>());
      return true;
    }
  } catch (...) {
    return false;
  }
  return false;
}

// {N} expands to 0..N-1; {start-end} to start..end; plain names pass through
inline std::vector<std::string> expand_topic_pattern(const std::string &pattern) {
  std::vector<std::string> result;

  const std::string::size_type open = pattern.find('{');
  if (open == std::string::npos) {
    result.push_back(pattern);
    return result;
  }
  const std::string::size_type close = pattern.find('}', open + 1);
  if (close == std::string::npos || close <= open + 1) {
    result.push_back(pattern);
    return result;
  }

  const std::string prefix = pattern.substr(0, open);
  const std::string suffix = pattern.substr(close + 1);
  const std::string inner = pattern.substr(open + 1, close - open - 1);

  int range_start = 0;
  int range_end = 0;
  const std::string::size_type dash = inner.find('-');
  try {
    if (dash != std::string::npos) {
      range_start = std::stoi(inner.substr(0, dash));
      range_end = std::stoi(inner.substr(dash + 1));
    } else {
      range_start = 0;
      range_end = std::stoi(inner) - 1;
    }
  } catch (...) {
    result.push_back(pattern);
    return result;
  }

  for (int i = range_start; i <= range_end; ++i) {
    result.push_back(prefix + std::to_string(i) + suffix);
  }
  return result;
}

inline bool parse_app_config(const std::string &json_config, AppConfig &out,
                             std::string &error) {
  json root;
  try {
    root = json::parse(json_config);
  } catch (const std::exception &e) {
    error = std::string("Config parse error: ") + e.what();
    return false;
  }

  if (!root.contains("services") || !root["services"].is_array()) {
    error = "Config must contain array field 'services'";
    return false;
  }

  AppConfig cfg;
  if (root.contains("id")) {
    try {
      if (root["id"].is_number_integer()) {
        cfg.test_id = root["id"].get<int>();
      } else if (root["id"].is_string()) {
        cfg.test_id = std::stoi(root["id"].get<std::string>());
      }
    } catch (...) {
    }
  }

  for (json::const_iterator svc_it = root["services"].begin(); svc_it != root["services"].end(); ++svc_it) {
    const json &svc_json = *svc_it;
    if (!svc_json.is_object()) {
      error = "Each service entry must be an object";
      return false;
    }

    ServiceConfig svc;
    svc.name = svc_json.value("name", std::string());
    svc.type = svc_json.value("type", std::string());
    svc.idx = svc_json.value("idx", 0);
    svc.type_index = svc_json.value("type_index", 0);
    svc.topic = svc_json.value("topic", std::string());

    if (svc_json.contains("topics") && svc_json["topics"].is_array()) {
      for (json::const_iterator t_it = svc_json["topics"].begin();
           t_it != svc_json["topics"].end(); ++t_it) {
        if (t_it->is_string()) {
          std::vector<std::string> expanded =
              expand_topic_pattern(t_it->get<std::string>());
          for (std::vector<std::string>::const_iterator e = expanded.begin();
               e != expanded.end(); ++e) {
            svc.topics.push_back(*e);
          }
        }
      }
    }

    if (svc.topics.empty() && !svc.topic.empty()) {
      svc.topics.push_back(svc.topic);
    }

    if (svc.type != "pub" && svc.type != "sub") {
      error = "Service type must be 'pub' or 'sub'";
      return false;
    }
    if (svc.topics.empty()) {
      error = "Service must specify 'topic' or non-empty 'topics'";
      return false;
    }

    if (svc.topic.empty()) {
      svc.topic = svc.topics.front();
    }

    if (svc_json.contains("parameters") && svc_json["parameters"].is_object()) {
      const json &params = svc_json["parameters"];

      int frequency = svc.frequency_hz;
      if (parse_int_param(params, "frequency", frequency) && frequency > 0) {
        svc.frequency_hz = frequency;
      }

      int size = svc.payload_size;
      bool size_found = parse_int_param(params, "size", size);
      if (!size_found && params.contains("data") && params["data"].is_object()) {
        size_found = parse_int_param(params["data"], "size", size);
      }
      if (size_found) {
        svc.payload_size = size;
      }

      int deadline = svc.aggregation_deadline_ms;
      bool deadline_found = parse_int_param(params, "aggregation_deadline_ms", deadline);
      if (!deadline_found) {
        deadline_found = parse_int_param(params, "deadline", deadline);
      }
      if (!deadline_found && params.contains("timing") && params["timing"].is_object()) {
        deadline_found = parse_int_param(params["timing"], "deadline", deadline);
      }
      if (!deadline_found && params.contains("transmission_timing") && params["transmission_timing"].is_object()) {
        deadline_found = parse_int_param(params["transmission_timing"], "deadline", deadline);
      }
      if (!deadline_found && params.contains("topic_topology") && params["topic_topology"].is_object()) {
        deadline_found = parse_int_param(params["topic_topology"], "deadline", deadline);
      }
      if (deadline_found && deadline > 0) {
        svc.aggregation_deadline_ms = deadline;
      }

      int offset = svc.publish_offset_us;
      if (parse_int_param(params, "publish_offset_us", offset) && offset >= 0) {
        svc.publish_offset_us = offset;
      }

      int timing_offset = svc.timing_offset_us;
      if (parse_int_param(params, "timing_offset_us", timing_offset) && timing_offset >= 0) {
        svc.timing_offset_us = timing_offset;
      }

      parse_string_param(params, "sending_pattern", svc.sending_pattern);

      if (params.contains("reliable") && params["reliable"].is_boolean()) {
        svc.reliable = params["reliable"].get<bool>();
      } else if (params.contains("reliability") && params["reliability"].is_string()) {
        const std::string r = params["reliability"].get<std::string>();
        svc.reliable = (r == "reliable" || r == "RELIABLE");
      }
    }

    if (svc.payload_size < 4) {
      error = "Payload size must be >= 4 bytes";
      return false;
    }

    cfg.services.push_back(svc);
  }

  if (cfg.services.empty()) {
    error = "Config contains no services";
    return false;
  }

  out = cfg;
  return true;
}

} // namespace tracer
