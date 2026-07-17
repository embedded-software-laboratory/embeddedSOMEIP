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

#ifndef SOMEIP_COMMUNICATION_LWIPTCPTRANSPORT_HPP
#define SOMEIP_COMMUNICATION_LWIPTCPTRANSPORT_HPP

#include <memory>

#include "someIp/communication/ITransport.hpp"
#include "someIp/communication/tcp/TcpDriver.hpp"

namespace someIp {

// ITransport backed by the lwIP TcpDriver
class LwipTcpTransport : public ITransport {
public:
  LwipTcpTransport();

  void init(const IpEndpoint &ref_local) override;
  void set_rx_callback(RxCallback cb) override;
  bool open_listen_port(uint16_t port) override;
  bool join_multicast_group(const IpEndpoint &) override { return false; }
  bool send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t src_port = 0) override;

  ConnectionId connect(const IpEndpoint &ref_dest) override;
  bool is_connected(const IpEndpoint &ref_dest) override;
  ConnectionId find_connection(const IpEndpoint &ref_dest) override;
  bool send_on(ConnectionId conn, BufferPtr buf) override;

  TransportKind kind() const override { return TransportKind::TCP; }

private:
  TcpDriver* m_ptr_drv = nullptr;
};

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_LWIPTCPTRANSPORT_HPP
