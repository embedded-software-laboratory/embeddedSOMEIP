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

#ifndef SOMEIP_SD_POOL_HPP
#define SOMEIP_SD_POOL_HPP

#include <utility>

#include "someIp/utils/PoolAllocator.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include "someIp/utils/PoolTrace.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp {
namespace sd {

// single shared pool for SD entry/option objects
using SdPool = FixedBlockPool<config::SD_POOL_BLOCK_SIZE, config::SD_POOL_BLOCKS>;

inline SdPool &sd_pool() {
  static SdPool pool;
  return pool;
}

// pool-backed PoolPtr, no heap, drop-in for make_shared
template <class T, class... Args>
PoolPtr<T> pool_make(Args &&...args) {
  return pool_ptr_make<T>(sd_pool(), pool_id::SD_ENTRY_OPTION, "sd_pool", std::forward<Args>(args)...);
}

// separate pool for larger SD objects (sd_message)
using SdLargePool = FixedBlockPool<config::SD_LARGE_POOL_BLOCK_SIZE, config::SD_LARGE_POOL_BLOCKS>;

inline SdLargePool &sd_large_pool() {
  static SdLargePool pool;
  return pool;
}

template <class T, class... Args>
PoolPtr<T> pool_make_large(Args &&...args) {
  return pool_ptr_make<T>(sd_large_pool(), pool_id::SD_LARGE, "sd_large_pool", std::forward<Args>(args)...);
}

// dedicated pool for sd_service, inline event and subscriber storage
using SdServicePool = FixedBlockPool<config::SD_SERVICE_POOL_BLOCK_SIZE, config::SD_SERVICE_POOL_BLOCKS>;

inline SdServicePool &sd_service_pool() {
  static SdServicePool pool;
  return pool;
}

template <class T, class... Args>
PoolPtr<T> pool_make_service(Args &&...args) {
  return pool_ptr_make<T>(sd_service_pool(), pool_id::SD_SERVICE, "sd_service_pool", std::forward<Args>(args)...);
}

} // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_POOL_HPP
