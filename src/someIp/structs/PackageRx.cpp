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

#include <memory>
#include <cstdio>
#include "PackageRx.hpp"
#include "someIp/config/config.hpp"
#include "someIp/handler/Parser.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/service_discovery/SdMessage.hpp"
#include "someIp/service_discovery/SdManager.hpp"
#include "someIp/service_discovery/SdPool.hpp"

namespace someIp
{

  PackageRx::PackageRx() : payload(), m_sourceIp{}, m_sourcePort(0) {};

PackageRx::PackageRx(PacketBuffer&& ref_package, const IpAddr sourceIp, uint16_t sourcePort, TcpConnection* ptr_conn)
  : payload(std::move(ref_package)), m_sourceIp(sourceIp), m_sourcePort(sourcePort), tcpConnection(ptr_conn) {
};

PackageRx::PackageRx(PacketBuffer&& ref_wrapper, rx_callback_fn callback, const IpAddr sourceIp, uint16_t sourcePort)
  : payload(std::move(ref_wrapper)), m_sourceIp(sourceIp), m_sourcePort(sourcePort), rx_callback(std::move(callback)), msg(sd::pool_make_large<sd::SdMessage>()) {
};

PackageRx::~PackageRx() {
};

PackageRx::PackageRx(PackageRx&& ref_other) noexcept
  : m_sourceIp(ref_other.m_sourceIp),
    m_sourcePort(ref_other.m_sourcePort),
    header(ref_other.header),
    payload(std::move(ref_other.payload)),
    tcpConnection(std::move(ref_other.tcpConnection)) {
  transportMode = ref_other.transportMode;
  ref_other.m_sourcePort = 0;
}

  PackageRx &PackageRx::operator=(PackageRx &&ref_other) noexcept
  {
    if (this != &ref_other)
    {
      m_sourceIp = ref_other.m_sourceIp;
      m_sourcePort = ref_other.m_sourcePort;
      header = ref_other.header;
      payload = std::move(ref_other.payload);

    tcpConnection = std::move(ref_other.tcpConnection); 
    transportMode = ref_other.transportMode;
    ref_other.m_sourcePort = 0;
  }
  return *this;
}

  bool PackageRx::deserialize(){
    if (!payload) {
      printf("ERROR: Null payload or buffer in packet!\n");
      return false;
    }
    Deserializer deser(payload.data(), payload.size());
    msg = sd::pool_make_large<sd::SdMessage>();
    if(!msg->deserialize(deser)){
      printf("Failed to deserialize message\n");
      return false;
    }
    return true;
  }

  ReturnCode PackageRx::extract_header_from_payload()
  {
    if (payload.size() < sizeof(someIp::Header))
    {
      PARSER_LOGGER::log("Buffer too small for Header.");
      return ReturnCode::E_NOT_OK;
    }

    if (payload.copy_out(&header, sizeof(someIp::Header), 0) != sizeof(someIp::Header))
    {
      PARSER_LOGGER::log("Failed to copy Header from payload.");
      return ReturnCode::E_NOT_OK;
    }
    header._message_id._service_id = someIp::ensure_host_order(header._message_id._service_id);
    header._message_id._method_id = someIp::ensure_host_order(header._message_id._method_id);
    header._request_id.client_id = someIp::ensure_host_order(header._request_id.client_id);
    header._request_id.session_id = someIp::ensure_host_order(header._request_id.session_id);
    PARSER_LOGGER::debug("Extracted message type: %d", (int)header._message_type);
    PARSER_LOGGER::debug("Extracted service id: %d", header._message_id._service_id);
    PARSER_LOGGER::debug("Extracted method id: %d", header._message_id._method_id);

    payload.remove_front(16);

  return ReturnCode::E_OK;
}

ReturnCode PackageRx::peek_header(Header* ptr_outHdr)
{
  if(!payload || payload.size() < sizeof(Header))
    return ReturnCode::E_NOT_OK;

  if (payload.copy_out(ptr_outHdr, sizeof(Header), 0) != sizeof(Header))
    return ReturnCode::E_NOT_OK;
  ptr_outHdr->_message_id._service_id = ensure_host_order(ptr_outHdr->_message_id._service_id);
  ptr_outHdr->_message_id._method_id  = ensure_host_order(ptr_outHdr->_message_id._method_id);
  ptr_outHdr->_request_id.client_id  = ensure_host_order(ptr_outHdr->_request_id.client_id);
  ptr_outHdr->_request_id.session_id = ensure_host_order(ptr_outHdr->_request_id.session_id);
  ptr_outHdr->_length                = ensure_host_order(ptr_outHdr->_length);

  return ReturnCode::E_OK;
}

// strip the 4 TP bytes and fix length and message type
ReturnCode PackageRx::strip_tp_header() // only for first segments
{
  // if (payload.get()->tot_len < sizeof(someIp::tp::TpHeader)) {
  //   PARSER_LOGGER::log("Buffer too small for tp Header.");
  //   return ReturnCode::E_NOT_OK;
  // }

  if (payload.size() < sizeof(someIp::Header)) { // must not pass back E_OK
    PARSER_LOGGER::log("Buffer too small for Header.");
    return ReturnCode::E_NOT_OK;
  }

  if (payload.size() < sizeof(someIp::Header)) {
    PARSER_LOGGER::log("Buffer too small for Header.");
    return ReturnCode::E_NOT_OK;
  }

  if(payload.size() < sizeof(someIp::tp::TpHeader)) // not a tp header
  {
    PARSER_LOGGER::log("strip_tp_header: not a tp header");
    return ReturnCode::E_OK;
  }


  extract_header_from_payload(); // fills the header member in host order

  if((static_cast<uint8_t>(this->header._message_type) & 0x20u) != 0u){
    if(!payload.remove_front(4))
    {
      PARSER_LOGGER::log("strip_tp_header: remove_front failed stripping tpOnlyHeader");
      return ReturnCode::E_NOT_OK;
    }
    this->header._message_type = static_cast<MessageType>(static_cast<uint8_t>(header._message_type) & ~0x20u); // 0b1101 1111 [SWS_SomeIpTp_00035]
    this->header._length -= 4;
  }

  // this->header.message_type = static_cast<MessageType>(static_cast<uint8_t>(header.message_type) & ~0x20u); // 0b1101 1111 [SWS_SomeIpTp_00035]
  // this->header.length -= 4;

  someIp::Header bigEndianHeader = (this->header);
  bigEndianHeader._message_id._service_id = someIp::ensure_network_order(bigEndianHeader._message_id._service_id);
  bigEndianHeader._message_id._method_id  = someIp::ensure_network_order(bigEndianHeader._message_id._method_id);
  bigEndianHeader._request_id.client_id  = someIp::ensure_network_order(bigEndianHeader._request_id.client_id);
  bigEndianHeader._request_id.session_id = someIp::ensure_network_order(bigEndianHeader._request_id.session_id);
  bigEndianHeader._length                = someIp::ensure_network_order(bigEndianHeader._length);

  payload.add_in_front(&bigEndianHeader, sizeof(bigEndianHeader)); // first segment needs the corrected BE header back in front
  return ReturnCode::E_OK;
}

ReturnCode PackageRx::delete_header()
{
  if (!payload) {
    PARSER_LOGGER::log("delete_header: null payload.");
    return ReturnCode::E_NOT_OK;
  }

  if (payload.size() < sizeof(someIp::tp::TpHeader)) {
    PARSER_LOGGER::log("Buffer too small for tp Header.");
    return ReturnCode::E_NOT_OK;
  }

  if (payload.size() < sizeof(someIp::tp::TpHeader)) {
    PARSER_LOGGER::log("not a tp Header.");
    return ReturnCode::E_NOT_OK;
  }

  if(!payload.remove_front(20))
  {
    PARSER_LOGGER::log("delete_header: remove_front failed stripping entire header");
    return ReturnCode::E_NOT_OK;
  }
  return ReturnCode::E_OK;
}

ReturnCode PackageRx::validate_protocol_version(){
  if(someIp::config::PROTOCOL_VERSION != header._protocol_version){
    return ReturnCode::E_WRONG_PROTOCOL_VERSION;
  }
  return ReturnCode::E_OK;
}

  ReturnCode PackageRx::validate_interface_version()
  {
    if (someIp::config::INTERFACE_VERSION != header._interface_version)
    {
      return ReturnCode::E_WRONG_INTERFACE_VERSION;
    }
    return ReturnCode::E_OK;
  }

} // namespace someIp
