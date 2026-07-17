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

#include <string.h>
#include "Package.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"
#include "someIp/config/config.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/logging/BaseLogger.hpp"

namespace {
constexpr char PACKAGE_TAG[] = "PACKAGE";
using PACKAGE_LOGGER = BaseLogger<PACKAGE_TAG>;
} // namespace

namespace someIp {

static PacketBuffer init_pbuf_wrapper(const uint8_t *ptr_data, size_t len){
  return PacketBuffer(ptr_data, len);
}

PacketBuffer init_pbuf_wrapper(std::string_view payload){
  return init_pbuf_wrapper(reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
}

Package::Package()
  : payload(init_pbuf_wrapper(""))
{

}

  Package::Package(uint16_t serviceId, uint16_t methodId, std::string_view payload, IpAddr destinationIp, uint16_t destinationPort)
      : header(), m_destinationIp(destinationIp), m_destinationPort(destinationPort), payload(init_pbuf_wrapper(payload))
  {
    header._message_id._service_id = serviceId;
    header._message_id._method_id = methodId;
    header._request_id.client_id = someIp::config::CLIENT_ID;
    header._request_id.session_id = 0; // TODO
    header._protocol_version = someIp::config::PROTOCOL_VERSION;
    header._interface_version = someIp::config::INTERFACE_VERSION;
    header._length = calculateLength();
    is_sd = false;
  }

  Package::Package(uint16_t serviceId, uint16_t methodId, const uint8_t *ptr_payload, size_t len, IpAddr destinationIp, uint16_t destinationPort)
      : header(), m_destinationIp(destinationIp), m_destinationPort(destinationPort), payload(init_pbuf_wrapper(ptr_payload, len))
  {
    header._message_id._service_id = serviceId;
    header._message_id._method_id = methodId;
    header._request_id.client_id = someIp::config::CLIENT_ID;
    header._request_id.session_id = 0; // TODO
    header._protocol_version = someIp::config::PROTOCOL_VERSION;
    header._interface_version = someIp::config::INTERFACE_VERSION;
    header._length = calculateLength();
    is_sd = false;
  }

  Package::Package(MessageId messageId, std::string_view payload, IpAddr destinationIp, uint16_t destinationPort)
      : header(), m_destinationIp(destinationIp), m_destinationPort(destinationPort), payload(init_pbuf_wrapper(payload))
  {
    header._message_id = messageId;
    header._request_id.client_id = someIp::config::CLIENT_ID;
    header._request_id.session_id = 0; // TODO
    header._protocol_version = someIp::config::PROTOCOL_VERSION;
    header._interface_version = someIp::config::INTERFACE_VERSION;
    header._return_code = ReturnCode::E_OK;
    header._length = calculateLength();
    is_sd = false;
  }

  Package::Package(sd::SdMessage msg, IpAddr destinationIp, uint16_t destinationPort, uint16_t src_port)
      : sdMessage(msg), m_destinationIp(destinationIp), m_destinationPort(destinationPort), payload(init_pbuf_wrapper("")), m_src_port(src_port)
  {
    // SD payload must stay empty or the datagram is corrupt
    is_sd = true;
  }

  Package::~Package() {}

  PacketBuffer Package::serialize()
  {
    if (is_sd)
    {
      return serialize_sd_package();
    }
    return serialize_package();
  }

  PacketBuffer Package::serialize_package()
  {
    someIp::Header bigEndianHeader = header;
    bigEndianHeader._message_id._service_id = someIp::ensure_network_order(header._message_id._service_id);
    bigEndianHeader._message_id._method_id = someIp::ensure_network_order(header._message_id._method_id);
    bigEndianHeader._request_id.client_id = someIp::ensure_network_order(header._request_id.client_id);
    bigEndianHeader._request_id.session_id = someIp::ensure_network_order(header._request_id.session_id);
    bigEndianHeader._length = someIp::ensure_network_order(calculateLength());
    // std::cout << "[SERIALIZE]: message type to serialize is: " << (int) bigEndianHeader.message_type << std::endl;
    // TODO: add debuger logger

    payload.add_in_front(&bigEndianHeader, sizeof(bigEndianHeader));

    return std::move(payload);
  }

  PacketBuffer Package::serialize_sd_package()
  {
    Serializer ser; // serializes into a fixed inline buffer, no heap
    sdMessage.serialize(ser);
    payload.add_in_front(ser.data(), ser.size());
    return std::move(payload);
  }

  int Package::size()
  {
    return sizeof(header) + sizeof(payload);
  }

} // namespace someIp
