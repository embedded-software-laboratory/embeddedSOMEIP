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

#include "utils/AllocCounter.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> g_armed{false};
std::atomic<size_t> g_count{0};
inline void tally() {
  if (g_armed.load(std::memory_order_relaxed)) {
    g_count.fetch_add(1, std::memory_order_relaxed);
  }
}
} // namespace

namespace someIp {
namespace test {
namespace alloc {

void arm() {
  g_count.store(0, std::memory_order_relaxed);
  g_armed.store(true, std::memory_order_relaxed);
}
void disarm() { g_armed.store(false, std::memory_order_relaxed); }
size_t count() { return g_count.load(std::memory_order_relaxed); }

} // namespace alloc
} // namespace test
} // namespace someIp

// global operator new/delete abort on OOM since build is -fno-exceptions
void *operator new(std::size_t n) {
  tally();
  void *p = std::malloc(n ? n : 1);
  if (!p) {
    std::abort();
  }
  return p;
}
void *operator new[](std::size_t n) {
  tally();
  void *p = std::malloc(n ? n : 1);
  if (!p) {
    std::abort();
  }
  return p;
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
