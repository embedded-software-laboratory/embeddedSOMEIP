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

#include "PackageTx.hpp"
#include "someIp/service_discovery/SdMessage.hpp"
#ifdef SOMEIP_PLATFORM_STM32
#include "someIp/communication/EndpointLwip.hpp"
#endif


namespace someIp
{
  PackageTx::PackageTx(PacketBuffer &&ref_payload, IpAddr ip, uint16_t port, uint16_t src_port_)
      : payload(std::move(ref_payload)),
        m_destIp(ip),
        m_destPort(port),
        m_src_port(src_port_)
  {
  }

PackageTx::PackageTx(PacketBuffer &&ref_payload, TcpConnection* ptr_tcpConn)
  : payload(std::move(ref_payload)), tcpConnection(ptr_tcpConn)
{
#ifdef SOMEIP_PLATFORM_STM32
  m_destIp = ptr_tcpConn ? ipaddr_from_lwip(ptr_tcpConn->pcb->remote_ip) : IpAddr();
  m_destPort = ptr_tcpConn ? ptr_tcpConn->pcb->remote_port : 0;
#else
  // POSIX transport sends via send_on(ConnectionId), no dest needed
  (void)ptr_tcpConn;
  m_destIp = IpAddr();
  m_destPort = 0;
#endif
}

PackageTx::~PackageTx(){}

PackageTx::PackageTx(PackageTx&& ref_other) noexcept 
: payload(std::move(ref_other.payload)), transportMode(ref_other.transportMode), tcpConnection(std::move(ref_other.tcpConnection)) {
  this->m_destIp = std::move(ref_other.m_destIp);
  this->m_destPort = std::move(ref_other.m_destPort);
  this->m_src_port = std::move(ref_other.m_src_port);

    ref_other.m_destIp = IpAddr();
    ref_other.m_destPort = 0;
  }

PackageTx& PackageTx::operator=(PackageTx&& ref_other) noexcept {
  if (this != &ref_other) {
    payload = std::move(ref_other.payload);
    m_destIp = std::move(ref_other.m_destIp);
    m_destPort = std::move(ref_other.m_destPort);
    transportMode = ref_other.transportMode;
    tcpConnection = std::move(ref_other.tcpConnection);  
    m_src_port = std::move(ref_other.m_src_port);
  }
  return *this;
}

} // namespace someIp
