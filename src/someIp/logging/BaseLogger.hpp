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

#ifndef BASE_LOGGER_HPP
#define BASE_LOGGER_HPP

#include <cstdio>
#include <utility>

#ifdef SOMEIP_PLATFORM_STM32
#endif
#include "someIp/utils/Lock.hpp"
#include "someIp/config/logging_config.hpp"

// disabled levels compile away via if constexpr
template <const char *Component>
class BaseLogger
{
public:
  template <typename... Args>
  static inline void trace(const char *ptr_fmt, Args &&...args)
  {
    if constexpr (someIp::config::TRACE_LOG_ENABLED)
    {
      std::printf("[%s - TRACE] ", Component);
      std::printf(ptr_fmt, std::forward<Args>(args)...);
      std::printf("\n");
    }
  }

  template <typename... Args>
  static inline void debug(const char *ptr_fmt, Args &&...args)
  {
    if constexpr (someIp::config::DEBUG_LOG_ENABLED)
    {
      std::printf("[%s - DEBUG] ", Component);
      std::printf(ptr_fmt, std::forward<Args>(args)...);
      std::printf("\n");
    }
  }

  template <typename... Args>
  static inline void debug_sd(const char *ptr_fmt, Args &&...args)
  {
    if constexpr (someIp::config::DEBUG_SD_LOG_ENABLED)
    {
      std::printf("[%s - DEBUG] ", Component);
      std::printf(ptr_fmt, std::forward<Args>(args)...);
      std::printf("\n");
    }
  }

  template <typename... Args>
  static inline void log(const char *ptr_fmt, Args &&...args)
  {
    if constexpr (someIp::config::NORMAL_LOG_ENABLED)
    {
      std::printf("[%s - INFO ] ", Component);
      std::printf(ptr_fmt, std::forward<Args>(args)...);
      std::printf("\n");
    }
  }

  template <typename... Args>
  static inline void error(const char *ptr_fmt, Args &&...args)
  {
    if constexpr (someIp::config::ERROR_LOG_ENABLED)
    {
      std::printf("[%s - ERROR] ", Component);
      std::printf(ptr_fmt, std::forward<Args>(args)...);
      std::printf("\n");
    }
  }
};

#endif // BASE_LOGGER_HPP
