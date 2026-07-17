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

#ifndef SOMEIP_UTILS_POOLTRACE_HPP
#define SOMEIP_UTILS_POOLTRACE_HPP

#include <cstdint>

// per-pool live/peak/total counters and LTTng tracepoints for PoolPtr
// defined in the cpp to keep LTTng out of PoolPtr.hpp

namespace someIp {

// COUNT bounds the static stats table, keep it above the pool count
namespace pool_id {
constexpr uint16_t SD_ENTRY_OPTION = 0; // sd_pool entries and options
constexpr uint16_t SD_LARGE        = 1; // sd_large_pool
constexpr uint16_t SD_SERVICE      = 2; // sd_service_pool
constexpr uint16_t TIMER           = 3; // TimerScheduler timer events
constexpr uint16_t COUNT           = 8; // capacity of the stats table

const char *name(uint16_t pool_id);
} // namespace pool_id

struct PoolStats {
  uint32_t live;
  uint32_t peak;  // high-water mark of live
  uint32_t total; // cumulative allocations
};

// thread-safe, never allocate
void pool_trace_on_alloc(uint16_t pool_id, const char *ptr_alloc_site);
void pool_trace_on_free(uint16_t pool_id, const char *ptr_alloc_site);

PoolStats pool_trace_stats(uint16_t pool_id);
uint32_t pool_trace_live(uint16_t pool_id);

} // namespace someIp

#endif // SOMEIP_UTILS_POOLTRACE_HPP
