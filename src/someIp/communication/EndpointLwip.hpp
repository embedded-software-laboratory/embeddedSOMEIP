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

#ifndef SOMEIP_COMMUNICATION_ENDPOINTLWIP_HPP
#define SOMEIP_COMMUNICATION_ENDPOINTLWIP_HPP

#include <lwip/ip_addr.h>
#include <lwip/def.h>

#include "someIp/communication/Endpoint.hpp"
#include "someIp/net/IpAddress.hpp"

namespace someIp {

// lwIP endpoint conversions, kept out of ITransport
inline IpEndpoint endpoint_from_lwip(const ip_addr_t &ref_a, uint16_t port) {
  uint32_t net = ip4_addr_get_u32(ip_2_ip4(const_cast<ip_addr_t *>(&ref_a)));
  return IpEndpoint(lwip_ntohl(net), port);
}

inline ip_addr_t endpoint_to_lwip(const IpEndpoint &ref_e) {
  ip_addr_t a;
  ip_addr_set_zero_ip4(&a);
  ip4_addr_set_u32(ip_2_ip4(&a), lwip_htonl(ref_e.v4));
  return a;
}

inline IpAddr ipaddr_from_lwip(const ip_addr_t &a) {
  uint32_t net = ip4_addr_get_u32(ip_2_ip4(const_cast<ip_addr_t *>(&a)));
  return IpAddr(lwip_ntohl(net));
}

inline ip_addr_t ipaddr_to_lwip(const IpAddr &ip) {
  ip_addr_t a;
  ip_addr_set_zero_ip4(&a);
  ip4_addr_set_u32(ip_2_ip4(&a), lwip_htonl(ip.v4));
  return a;
}

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_ENDPOINTLWIP_HPP
