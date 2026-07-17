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

#ifndef SOMEIP_COMMUNICATION_LWIPUDPTRANSPORT_HPP
#define SOMEIP_COMMUNICATION_LWIPUDPTRANSPORT_HPP

#include <memory>

#include "someIp/communication/ITransport.hpp"
#include "someIp/communication/udp/UdpDriver.hpp"

namespace someIp {

// ITransport backed by the lwIP UdpDriver
class LwipUdpTransport : public ITransport {
public:
  LwipUdpTransport();

  void init(const IpEndpoint &ref_local) override;
  void set_rx_callback(RxCallback cb) override;
  bool open_listen_port(uint16_t port) override;
  bool join_multicast_group(const IpEndpoint &ref_group) override;
  bool send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t src_port = 0) override;

  // stream methods not applicable to a datagram transport
  ConnectionId connect(const IpEndpoint &) override { return {}; }
  bool is_connected(const IpEndpoint &) override { return false; }
  ConnectionId find_connection(const IpEndpoint &) override { return {}; }
  bool send_on(ConnectionId, BufferPtr) override { return false; }

  TransportKind kind() const override { return TransportKind::UDP; }

private:
  UdpDriver* m_ptr_drv = nullptr;
};

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_LWIPUDPTRANSPORT_HPP
