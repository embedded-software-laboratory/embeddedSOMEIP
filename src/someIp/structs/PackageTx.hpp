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

#ifndef SOMEIP_PACKAGE_TX_HPP
#define SOMEIP_PACKAGE_TX_HPP

#include <string>
#include "someIp/utils/PoolPtr.hpp"
#include <memory>

#include "someIp/net/IpAddress.hpp"
#include "Header.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/config/communication_config.hpp"
#include "someIp/config/config.hpp"

namespace someIp {
  namespace sd {
    class SdMessage;
  }
class PackageTx {
  public:
  PackageTx() = default;

  // default 0 means the caller's own app port, SD passes SD_PORT
  PackageTx(PacketBuffer &&ref_payload, IpAddr ip, uint16_t port, uint16_t src_port_ = 0);

  PackageTx(PacketBuffer &&ref_payload, TcpConnection* ptr_tcpConn);

  PackageTx(someIp::PoolPtr<sd::SdMessage> sprt_msg, IpAddr ip, uint16_t port, uint16_t src_port_);

  ~PackageTx();

  PackageTx(const PackageTx&) = delete;
  PackageTx& operator=(const PackageTx&) = delete;

  PackageTx(PackageTx&& ref_other) noexcept;
  PackageTx& operator=(PackageTx&& ref_other) noexcept;

  PacketBuffer payload;

  TcpConnection *tcpConnection = nullptr; 

  TransportKind transportMode = TransportKind::UDP_TP; // set in the send functions

  TransportKind get_transport_mode() { return this->transportMode; }
  const IpAddr* get_dest_ip() const { return &m_destIp; }
  uint16_t get_dest_port() const { return m_destPort; }
  uint16_t get_src_port() const { return m_src_port; }

  private:
  IpAddr m_destIp;

  uint16_t m_destPort;

  uint16_t m_src_port;

  someIp::PoolPtr<sd::SdMessage> m_sprt_sd_msg;
};
}

#endif // SOMEIP_PACKAGE_TX_HPP
