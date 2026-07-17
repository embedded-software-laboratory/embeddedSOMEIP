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

#include "someIp/communication/LwipTcpTransport.hpp"
#include "someIp/communication/EndpointLwip.hpp"
#include "someIp/communication/PbufByteBuffer.hpp"

namespace someIp {

LwipTcpTransport::LwipTcpTransport() : m_ptr_drv(TcpDriver::get_instance()) {}

void LwipTcpTransport::init(const IpEndpoint &ref_local) {
  ip_addr_t l = endpoint_to_lwip(ref_local);
  m_ptr_drv->init(nullptr, nullptr, l);
}

void LwipTcpTransport::set_rx_callback(RxCallback cb) {
  TcpDriver::set_rx_sink(std::move(cb));
}

bool LwipTcpTransport::open_listen_port(uint16_t port) {
  return m_ptr_drv->create_server_tcp_connection(port) != nullptr;
}

bool LwipTcpTransport::send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t /*src_port*/) {
  ip_addr_t d = endpoint_to_lwip(ref_dest);
  ip4_addr_t d4 = *ip_2_ip4(&d);
  PbufWrapper w = take_pbuf(std::move(buf));
  if (w.get() == nullptr) {
    return false;
  }
  return m_ptr_drv->send_tcp_packet(d4, ref_dest.port, std::move(w));
}

ConnectionId LwipTcpTransport::connect(const IpEndpoint &ref_dest) {
  ip_addr_t d = endpoint_to_lwip(ref_dest);
  TcpConnection *ptr_conn = m_ptr_drv->create_client_tcp_connection(d, ref_dest.port, nullptr);
  return ptr_conn ? ConnectionId(TransportKind::TCP, ptr_conn) : ConnectionId();
}

bool LwipTcpTransport::is_connected(const IpEndpoint &ref_dest) {
  ip_addr_t d = endpoint_to_lwip(ref_dest);
  return m_ptr_drv->is_connected(d, ref_dest.port);
}

ConnectionId LwipTcpTransport::find_connection(const IpEndpoint &ref_dest) {
  ip_addr_t d = endpoint_to_lwip(ref_dest);
  TcpConnection *ptr_conn = m_ptr_drv->find_tcp_connection(d, ref_dest.port);
  return ptr_conn ? ConnectionId(TransportKind::TCP, ptr_conn) : ConnectionId();
}

bool LwipTcpTransport::send_on(ConnectionId conn, BufferPtr buf) {
  if (conn.kind != TransportKind::TCP || conn.handle == nullptr) {
    return false;
  }
  PbufWrapper w = take_pbuf(std::move(buf));
  if (w.get() == nullptr) {
    return false;
  }
  return m_ptr_drv->send_tcp_packet_pcb(static_cast<TcpConnection *>(conn.handle), std::move(w));
}

} // namespace someIp
