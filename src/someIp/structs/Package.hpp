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

#ifndef SOMEIP_PACKAGE_HPP
#define SOMEIP_PACKAGE_HPP

#include <string.h>
#include <string_view>
#include <memory>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "Header.hpp"
#include "someIp/service_discovery/SdMessage.hpp"

namespace someIp
{

  class Package
  {
  public:
    Package(); // = default; // TOOD: check with default
    Package(uint16_t serviceId, uint16_t methodId, std::string_view payload, IpAddr destinationIp, uint16_t destinationPort);
    Package(uint16_t serviceId, uint16_t methodId, const uint8_t *ptr_payload, size_t len, IpAddr destinationIp, uint16_t destinationPort);
    Package(MessageId messageId, std::string_view payload, IpAddr destinationIp, uint16_t destinationPort);
    Package(sd::SdMessage msg, IpAddr destinationIp, uint16_t destinationPort, uint16_t src_port = someIp::config::SD_PORT);

    ~Package();

    PacketBuffer serialize();

    PacketBuffer serialize_package();

    PacketBuffer serialize_sd_package();

    int size();

    bool is_sd = false;

    Header header;

    sd::SdMessage sdMessage;

    PacketBuffer payload;

    uint16_t get_session_id() const
    {
      return header._request_id.session_id;
    }

    void set_session_id(uint16_t sessionId)
    {
      header._request_id.session_id = sessionId;
    }

    uint16_t getDestinationPort() const
    {
      return m_destinationPort;
    }

    IpAddr getDestinationIp() const
    {
      return m_destinationIp;
    }

    // notifications must use the offered service port
    uint16_t getSrcPort() const { return m_src_port; }
    void setSrcPort(uint16_t port) { m_src_port = port; }

  uint32_t calculateLength() {
    if(!this->payload){
      return 8;
    }
    return 8 + this->payload.size(); // 8 bytes for the header
  }

  private:
    IpAddr m_destinationIp;

    uint16_t m_destinationPort;

    uint16_t m_src_port = 0;
  };
}

#endif // SOMEIP_PACKAGE_HPP
