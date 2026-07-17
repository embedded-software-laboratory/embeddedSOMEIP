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

#ifndef SOMEIP_NET_IPADDRESS_HPP
#define SOMEIP_NET_IPADDRESS_HPP

#include <cstdint>
#include <cstddef>
#include <cstdio>

namespace someIp {

// transport-neutral IPv4, lwIP transports convert at their boundary
struct IpAddr {
  uint32_t v4 = 0; // IPv4 address, host byte order

  IpAddr() = default;
  explicit IpAddr(uint32_t host_order) : v4(host_order) {}

  static IpAddr any() { return IpAddr(0); }
  static IpAddr from_u32(uint32_t host_order) { return IpAddr(host_order); }
  uint32_t to_u32() const { return v4; }
  bool is_any() const { return v4 == 0; }

  bool operator==(const IpAddr &o) const { return v4 == o.v4; }
  bool operator!=(const IpAddr &o) const { return v4 != o.v4; }

  // writes "a.b.c.d" into buf, returns buf
  const char *to_string(char *buf, size_t n) const {
    std::snprintf(buf, n, "%u.%u.%u.%u", (unsigned)((v4 >> 24) & 0xFF),
                  (unsigned)((v4 >> 16) & 0xFF), (unsigned)((v4 >> 8) & 0xFF),
                  (unsigned)(v4 & 0xFF));
    return buf;
  }

  // shared static buffer, not thread-safe, like lwIP ip4addr_ntoa
  const char *c_str() const {
    static char buf[16];
    return to_string(buf, sizeof(buf));
  }

  // parses "a.b.c.d", false on malformed input
  static bool from_string(const char *s, IpAddr &out) {
    if (!s) return false;
    unsigned a, b, c, d;
    if (std::sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if ((a | b | c | d) > 255u) return false;
    out.v4 = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
  }
  static IpAddr from_string(const char *s) {
    IpAddr out;
    from_string(s, out);
    return out;
  }
};

} // namespace someIp

#endif // SOMEIP_NET_IPADDRESS_HPP
