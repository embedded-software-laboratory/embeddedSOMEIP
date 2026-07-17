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
#ifndef SOMEIP_UDP_CONNECTION_HPP
#define SOMEIP_UDP_CONNECTION_HPP

#include "lwip/udp.h"
#include "someIp/communication/TcpipCoreLock.hpp"

namespace someIp {

struct UdpConnection
{
  udp_pcb *pcb = nullptr;
  uint16_t port = 0;

  UdpConnection() = default;

  explicit UdpConnection(uint16_t port) : port(port) {
    TcpipCoreLock lock;
    pcb = udp_new();
  }

  UdpConnection &operator=(UdpConnection &&ref_other) noexcept {
    port = ref_other.port;

    if (pcb != nullptr) {
      TcpipCoreLock lock;
      udp_remove(pcb);
    }
    pcb = ref_other.pcb;
    ref_other.pcb = nullptr;
    return *this;
  }

  ~UdpConnection() {
    if (pcb != nullptr) {
      TcpipCoreLock lock;
      udp_remove(pcb);
      pcb = nullptr;
    }
  }
};
}

#endif // SOMEIP_UDP_CONNECTION_HPP