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

#include "someIp/communication/LwipUdpTransport.hpp"
#include "someIp/communication/EndpointLwip.hpp"
#include "someIp/communication/PbufByteBuffer.hpp"
#include "someIp/config/config.hpp"

namespace someIp {

LwipUdpTransport::LwipUdpTransport() : m_ptr_drv(UdpDriver::get_instance()) {}

void LwipUdpTransport::init(const IpEndpoint &ref_local) {
  ip_addr_t l = endpoint_to_lwip(ref_local);
  // callback args vestigial, RX flows through the static sink
  m_ptr_drv->init(nullptr, nullptr, l);
}

void LwipUdpTransport::set_rx_callback(RxCallback cb) {
  UdpDriver::set_rx_sink(std::move(cb));
}

bool LwipUdpTransport::open_listen_port(uint16_t port) {
  return m_ptr_drv->create_udp_connection(port) != nullptr;
}

bool LwipUdpTransport::join_multicast_group(const IpEndpoint &ref_group) {
  ip_addr_t g = endpoint_to_lwip(ref_group);
  m_ptr_drv->join_multicast_group(g);
  return true;
}

bool LwipUdpTransport::send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t src_port) {
  ip_addr_t d = endpoint_to_lwip(ref_dest);
  PbufWrapper w = take_pbuf(std::move(buf));
  if (w.get() == nullptr) {
    return false;
  }
  // keep the source port so a RESPONSE leaves the port the request hit
  bool error;
  if (src_port != 0) {
    error = m_ptr_drv->send_udp_packet(d, ref_dest.port, std::move(w), src_port);
  } else {
    error = m_ptr_drv->send_udp_packet(d, ref_dest.port, std::move(w));
  }
  return !error;
}

} // namespace someIp
