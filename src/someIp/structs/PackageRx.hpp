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

#ifndef SOMEIP_PACKAGE_RX_HPP
#define SOMEIP_PACKAGE_RX_HPP

#include <string>
#include "someIp/utils/PoolPtr.hpp"
#include <memory>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "Header.hpp"
#include "someIp/structs/TPHeader.hpp"
#include "someIp/config/communication_config.hpp"
#include "someIp/communication/tcp/TcpConnection.hpp"

#include <functional>


namespace someIp
{
  namespace sd{
    class SdManager;
    class SdMessage;
  }

  class PackageRx
  {
  public:
    using rx_callback_fn = std::function<void(sd::SdManager*, sd::SdMessage, IpAddr, uint16_t)>;
    rx_callback_fn rx_callback;

    PackageRx();
    PackageRx(PacketBuffer &&ref_package, IpAddr sourceIp, uint16_t sourcePort, TcpConnection* ptr_conn = nullptr);
    PackageRx(PacketBuffer &&ref_wrapper, rx_callback_fn callback, IpAddr sourceIp, uint16_t sourcePort);
    ~PackageRx();

    PackageRx(const PackageRx &) = delete;
    PackageRx &operator=(const PackageRx &) = delete;

    PackageRx(PackageRx &&ref_other) noexcept;
    PackageRx &operator=(PackageRx &&ref_other) noexcept;

  // inline value, no per-message heap allocation
  Header header{};

    PacketBuffer payload;

    someIp::PoolPtr<sd::SdMessage> msg;

    TransportKind transportMode = TransportKind::DEFAULT;

    TcpConnection* tcpConnection = nullptr;

    IpAddr get_source_ip() const { return m_sourceIp; }
    uint16_t get_source_port() const { return m_sourcePort; }

  const char* get_ip_as_string(){
    return m_sourceIp.c_str();
  }

  // host-order copy of the first 16 bytes, payload untouched
  ReturnCode peek_header(Header* ptr_outHdr);

  // strip the 4 TP bytes, leaving the SOME/IP part
  ReturnCode strip_tp_header(); // only suitable for first segs

  ReturnCode delete_header();

  // run before any header-dependent function
  ReturnCode extract_header_from_payload();

  bool deserialize();

    ReturnCode validate_protocol_version();

    ReturnCode validate_interface_version();

  private:
    IpAddr m_sourceIp;

    uint16_t m_sourcePort;
  };
}

#endif // SOMEIP_PACKAGE_RX_HPP
