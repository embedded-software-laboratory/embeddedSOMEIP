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

#include <gtest/gtest.h>
#include <string>
#include <cstdint>
#include <cstring>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/structs/Header.hpp"
#include "someIp/enums/MessageType.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/transport-protocol/TpPackage.hpp"
#include "someIp/structs/TPHeader.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"


using someIp::tp::TpPackage; 
using someIp::tp::TpHeader;
using someIp::Header; 

static someIp::IpAddr makeip(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return someIp::IpAddr::from_u32(((uint32_t)a << 24) | ((uint32_t)b << 16) |
                                    ((uint32_t)c << 8) | d);
}

TEST(TpPackage, Length_NoPayload) 
{
    TpPackage pkg; 
    EXPECT_EQ(pkg.calculateLength(),12);  
}

TEST(TpPackage, Length_EmptyPayload) 
{
    // explicit zero length buffer
    someIp::PacketBuffer wrapper{};
    Header base{};
    someIp::IpAddr ip = makeip(1,2,3,4);

    TpPackage pkg(base, std::move(wrapper), ip, 4242, 0, false);
    EXPECT_EQ(pkg.calculateLength(),12); 
}

TEST(TpPackage, Length_NonEmptyPbuf)
{
    const std::string payload = "hello TP!";
    someIp::PacketBuffer wrapper(payload.data(), payload.size());

    Header base{};
    someIp::IpAddr ip = makeip(1,2,3,4);
    TpPackage pkg(base, std::move(wrapper), ip, 5555, 0, true);

    
    EXPECT_EQ(pkg.calculateLength(),12 + payload.size());
}

TEST(TpPackage, Serialize_NetworkEndianFieldsAndTpOnlyHeader)
{
    // build a payload
    const std::string payload = "ABC";
    someIp::PacketBuffer wrapper(payload.data(), payload.size());

    // fill base someIp Header
    Header base;

    someIp::RequestId requestId;
    requestId.client_id = 0x0003;
    requestId.session_id = 0x0004;

    someIp::MessageId messageId;
    messageId._service_id = 0x0001;
    messageId._method_id = 0x0002;

    base._message_id = messageId;
    base._request_id = requestId;
    base._protocol_version = 0x4D;
    base._interface_version = 0x4E;
    base._message_type = MessageType::REQUEST;
    base._return_code = ReturnCode::E_OK;

    someIp::IpAddr ip = makeip(10,20,30,40);
    uint16_t port = 3030;
    uint32_t offset = 7;
    bool msf = true;

    TpPackage  pkg(base, std::move(wrapper), ip, port, offset, msf);

    // check correctness of first construction
    EXPECT_EQ(pkg.tpheader.message_id._service_id, messageId._service_id);
    EXPECT_EQ(pkg.tpheader.message_id._method_id, messageId._method_id);
    EXPECT_EQ(pkg.tpheader.request_id.client_id, requestId.client_id);
    EXPECT_EQ(pkg.tpheader.request_id.session_id, requestId.session_id);
    EXPECT_EQ(pkg.tpheader.protocol_version, base._protocol_version);
    EXPECT_EQ(pkg.tpheader.interface_version, base._interface_version);
    EXPECT_EQ(pkg.tpheader.message_type, MessageType::REQUEST);
    EXPECT_EQ(pkg.tpheader.return_code, ReturnCode::E_OK);
    EXPECT_EQ(pkg.tpheader.tponlyheader.get_offset(), offset);
    EXPECT_EQ(pkg.tpheader.tponlyheader.get_reserved(), 0);
    EXPECT_EQ(pkg.tpheader.tponlyheader.get_msf(), msf);
    EXPECT_EQ(pkg.tpheader.length, 12 + payload.size());
    EXPECT_EQ(pkg.getDestinationPort(), port);

    // serialize and inspect Header bytes
    uint32_t expected_len = pkg.calculateLength(); // payload is moved out by serialize
    auto chain = pkg.serialize();
    ASSERT_TRUE(chain);
    const uint8_t* data = chain.data();
    EXPECT_EQ(chain.size(), sizeof(TpHeader) + payload.size());

    // service and method id
    uint16_t be_sid = (data[0]<<8) | data[1];
    uint16_t be_mid = (data[2]<<8) | data[3];
    EXPECT_EQ(be_sid, 1);   
    EXPECT_EQ(be_mid, 2);  

    // length field big endian at offset 4 to 7
    uint32_t reported_length = (uint32_t(data[4])<<24) | (uint32_t(data[5])<<16) | (uint32_t(data[6])<<8) | uint32_t(data[7]);
    EXPECT_EQ(reported_length, expected_len);

    // request id client and session at 8 to 11
    uint16_t be_client = (uint16_t(data[8])<<8) | uint16_t(data[9]); 
    uint16_t be_session= (uint16_t(data[10])<<8)| uint16_t(data[11]); 
    EXPECT_EQ(be_client, 3);
    EXPECT_EQ(be_session, 4); 

    // protocol/interface/Type/returncode
    EXPECT_EQ(data[12],base._protocol_version); 
    EXPECT_EQ(data[13],base._interface_version); 
    EXPECT_EQ(data[14],static_cast<uint8_t>(MessageType::REQUEST)); 
    EXPECT_EQ(data[15],static_cast<uint8_t>(ReturnCode::E_OK)); 

    //Tponlyheader (big endian)
    uint32_t tp_full = (uint32_t(data[16])<<24) | (uint32_t(data[17])<<16) | (uint32_t(data[18])<<8) | uint32_t(data[19]); 
    uint32_t expected_tp = (offset<<4) | (msf ? 1 : 0); 
    EXPECT_EQ(tp_full, expected_tp); 

    //payload follows the header in the same flat buffer
    EXPECT_EQ(std::memcmp(data + sizeof(TpHeader), payload.data(), payload.size()), 0);
}


