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
#include "TpPackage.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"
#include "someIp/config/config.hpp"
#include "someIp/logging/BaseLogger.hpp"

namespace {
constexpr char TP_PACKAGE_TAG[] = "TP_PKG";
using TP_PACKAGE_LOGGER = BaseLogger<TP_PACKAGE_TAG>;
} // namespace

namespace someIp
{
    namespace tp
    {
        PacketBuffer init_pbuf_wrapper_tp(const std::string& ref_payload)
        {
            return PacketBuffer(reinterpret_cast<const uint8_t*>(ref_payload.data()), ref_payload.size());
        }

        //Only neccessary one
        TpPackage::TpPackage(const Header &ref_baseHeader, PacketBuffer&& ref_payload, IpAddr& ref_destinationIp, uint16_t destinationPort, uint32_t offset, bool moreSegmentsFlag)
            : payload(std::move(ref_payload)),m_destinationIp(ref_destinationIp),m_destinationPort(destinationPort)
        {
                tpheader.message_id        = ref_baseHeader._message_id;
                tpheader.request_id        = ref_baseHeader._request_id;
                tpheader.protocol_version  = ref_baseHeader._protocol_version;
                tpheader.interface_version = ref_baseHeader._interface_version;
                tpheader.message_type      = ref_baseHeader._message_type;
                tpheader.return_code       = ref_baseHeader._return_code;
            
                tpheader.tponlyheader.set_offset(offset);
                tpheader.tponlyheader.set_reserved(0); // [SWS_SomeIpTp_00013]
                tpheader.tponlyheader.set_msf(moreSegmentsFlag);

                tpheader.length = calculateLength();
        }

        TpPackage::~TpPackage() {}

        

        // NOTE only call once, this moves the payload
        PacketBuffer TpPackage::serialize()
        {
            if(!payload)
            {
                TP_PACKAGE_LOGGER::error("TpPackage: serialize: payload is null");
                return  std::move(payload);
            }
            TpHeader bigEndianHeader = tpheader;
            bigEndianHeader.message_id._service_id = someIp::ensure_network_order(tpheader.message_id._service_id);
            bigEndianHeader.message_id._method_id  = someIp::ensure_network_order(tpheader.message_id._method_id);
            bigEndianHeader.request_id.client_id  = someIp::ensure_network_order(tpheader.request_id.client_id);
            bigEndianHeader.request_id.session_id = someIp::ensure_network_order(tpheader.request_id.session_id);
            bigEndianHeader.length                = someIp::ensure_network_order(tpheader.length);

            bigEndianHeader.tponlyheader.full_tponlyheader = someIp::ensure_network_order(tpheader.tponlyheader.full_tponlyheader);

            payload.add_in_front(&bigEndianHeader, sizeof(bigEndianHeader));

            return std::move(payload);
        }

        int TpPackage::size()
        {
            return sizeof(tpheader) + sizeof(payload);
        }

    } // namespace tp
} // namespace someIp
