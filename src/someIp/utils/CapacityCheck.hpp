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

#ifndef SOMEIP_UTILS_CAPACITYCHECK_HPP
#define SOMEIP_UTILS_CAPACITYCHECK_HPP

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "someIp/config/StackConfig.hpp"

namespace someIp {

// the fatal policy aborts, otherwise callers must degrade
inline void capacity_exhausted(const char *ptr_what) {
  std::printf("[CAPACITY] error: fixed capacity exhausted: %s\n", ptr_what);
#if SOMEIP_POOL_EXHAUSTION_FATAL
  std::abort();
#endif
}

} // namespace someIp

// overflow guard, aborts under the fatal policy and asserts in debug builds
#define SOMEIP_CAPACITY_FAIL(tag)                                              \
  do {                                                                         \
    ::someIp::capacity_exhausted(tag);                                         \
    assert(false && "someIp fixed-capacity overflow (see [CAPACITY] log)");    \
  } while (0)

#endif // SOMEIP_UTILS_CAPACITYCHECK_HPP
