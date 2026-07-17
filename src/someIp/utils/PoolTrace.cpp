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

#include "someIp/utils/PoolTrace.hpp"

#include <atomic>

#include "someip_tp.h"

namespace someIp {

namespace pool_id {
const char *name(uint16_t pool_id) {
  switch (pool_id) {
    case SD_ENTRY_OPTION: return "sd_entry_option";
    case SD_LARGE:        return "sd_large";
    case SD_SERVICE:      return "sd_service";
    case TIMER:           return "timer";
    default:              return "unknown";
  }
}
} // namespace pool_id

namespace {
// static atomics, valid before any constructor runs
std::atomic<uint32_t> g_live[pool_id::COUNT];
std::atomic<uint32_t> g_peak[pool_id::COUNT];
std::atomic<uint32_t> g_total[pool_id::COUNT];

inline bool in_range(uint16_t pool_id) { return pool_id < pool_id::COUNT; }
} // namespace

void pool_trace_on_alloc(uint16_t pool_id, const char *ptr_alloc_site) {
  if (!in_range(pool_id)) return;
  const uint32_t live = g_live[pool_id].fetch_add(1, std::memory_order_relaxed) + 1;
  g_total[pool_id].fetch_add(1, std::memory_order_relaxed);
  // high-water mark, a benign race just under-reports
  uint32_t peak = g_peak[pool_id].load(std::memory_order_relaxed);
  while (live > peak && !g_peak[pool_id].compare_exchange_weak(peak, live, std::memory_order_relaxed)) {
  }
  lttng_ust_tracepoint(someip, pool_alloc, pool_id, ptr_alloc_site ? ptr_alloc_site : "?", live);
}

void pool_trace_on_free(uint16_t pool_id, const char *ptr_alloc_site) {
  if (!in_range(pool_id)) return;
  const uint32_t live = g_live[pool_id].fetch_sub(1, std::memory_order_relaxed) - 1;
  lttng_ust_tracepoint(someip, pool_free, pool_id, ptr_alloc_site ? ptr_alloc_site : "?", live);
}

PoolStats pool_trace_stats(uint16_t pool_id) {
  if (!in_range(pool_id)) return {0, 0, 0};
  return {g_live[pool_id].load(std::memory_order_relaxed),
          g_peak[pool_id].load(std::memory_order_relaxed),
          g_total[pool_id].load(std::memory_order_relaxed)};
}

uint32_t pool_trace_live(uint16_t pool_id) {
  return in_range(pool_id) ? g_live[pool_id].load(std::memory_order_relaxed) : 0;
}

} // namespace someIp
