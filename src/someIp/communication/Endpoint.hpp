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

#ifndef SOMEIP_COMMUNICATION_ENDPOINT_HPP
#define SOMEIP_COMMUNICATION_ENDPOINT_HPP

#include <cstdint>

namespace someIp {

// backend-neutral IPv4 endpoint, no lwIP or POSIX types
struct IpEndpoint {
  uint32_t v4 = 0;   // IPv4 address, host byte order
  uint16_t port = 0; // UDP/TCP port, host byte order

  IpEndpoint() = default;
  IpEndpoint(uint32_t addr_host_order, uint16_t port_) : v4(addr_host_order), port(port_) {}

  bool operator==(const IpEndpoint &ref_o) const { return v4 == ref_o.v4 && port == ref_o.port; }
  bool operator!=(const IpEndpoint &ref_o) const { return !(*this == ref_o); }
};

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_ENDPOINT_HPP
