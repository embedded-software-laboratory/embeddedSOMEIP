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

#ifndef NETWORK_BYTE_ORDER_CONVERTER_HPP
#define NETWORK_BYTE_ORDER_CONVERTER_HPP

#include <cstdint>
#include <cstring>

// Network byte order = big endian. Dependency-free helpers (no lwIP, no
// <arpa/inet.h>) so the same code compiles on STM32 and POSIX hosts.

namespace someIp
{

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  static inline uint16_t byteorder_swap16(uint16_t v) { return v; }
  static inline uint32_t byteorder_swap32(uint32_t v) { return v; }
  static inline bool is_big_endian(void) { return true; }
#else
  static inline uint16_t byteorder_swap16(uint16_t v) { return __builtin_bswap16(v); }
  static inline uint32_t byteorder_swap32(uint32_t v) { return __builtin_bswap32(v); }
  static inline bool is_big_endian(void) { return false; }
#endif

  // alignment-safe read of a big-endian uint16, returned in host order
  static inline uint16_t read_u16_be(const void *ptr_base, size_t offset)
  {
    uint16_t be;
    std::memcpy(&be, static_cast<const uint8_t *>(ptr_base) + offset, sizeof(be));
    return byteorder_swap16(be);
  }

  static inline uint16_t to_big_endian_16(uint16_t value) { return byteorder_swap16(value); }
  static inline uint32_t to_big_endian_32(uint32_t value) { return byteorder_swap32(value); }
  static inline uint16_t to_little_endian_16(uint16_t value) { return byteorder_swap16(value); }
  static inline uint32_t to_little_endian_32(uint32_t value) { return byteorder_swap32(value); }

  static inline uint16_t ensure_network_order(uint16_t input) { return byteorder_swap16(input); }
  static inline uint32_t ensure_network_order(uint32_t input) { return byteorder_swap32(input); }
  static inline uint16_t ensure_host_order(uint16_t input) { return byteorder_swap16(input); }
  static inline uint32_t ensure_host_order(uint32_t input) { return byteorder_swap32(input); }

}

#endif // NETWORK_BYTE_ORDER_CONVERTER_HPP
