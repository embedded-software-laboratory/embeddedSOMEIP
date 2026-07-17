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
#include <cstring>

#include "someIp/structs/TPHeader.hpp"
#include "someIp/enums/MessageType.hpp"
#include "someIp/enums/ReturnCode.hpp"

using someIp::tp::TpOnlyHeader; 
using someIp::tp::TpHeader;

// one uint32_t packs 28-bit offset, 3-bit reserved, 1-bit msf
TEST(TpOnlyHeader, DefaultValues)
{
    TpOnlyHeader h; 
    EXPECT_EQ(h.full_tponlyheader,0);
    EXPECT_EQ(h.get_offset(),0); 
    EXPECT_EQ(h.get_reserved(),0); 
    EXPECT_FALSE(h.get_msf()); 
}

TEST(TpOnlyHeader, SetAndGetFields)
{
    TpOnlyHeader h;
    h.set_offset(0x1234567);
    EXPECT_EQ(h.get_offset(),0x1234567);

    h.set_reserved(0x07);
    EXPECT_EQ(h.get_reserved(),0x7);

    h.set_msf(true);
    EXPECT_TRUE(h.get_msf());
    h.set_msf(false); 
    EXPECT_FALSE(h.get_msf());
}

TEST(TpHeader, SizeInBytes)
{
    //MessageID(4)+ length(4) + RequesId(4)+ proto(1) + iface(1) + type(1) + code(1) + tponly(4) = 20 
    TpHeader header;

    EXPECT_EQ(sizeof(header), 20);
    EXPECT_EQ(sizeof(header.message_id), 4);
    EXPECT_EQ(sizeof(header.message_id._method_id), 2);
    EXPECT_EQ(sizeof(header.message_id._service_id), 2);
    EXPECT_EQ(sizeof(header.length), 4);
    EXPECT_EQ(sizeof(header.request_id), 4);
    EXPECT_EQ(sizeof(header.request_id.client_id), 2);
    EXPECT_EQ(sizeof(header.request_id.session_id), 2);
    EXPECT_EQ(sizeof(header.protocol_version), 1);
    EXPECT_EQ(sizeof(header.interface_version), 1);
    EXPECT_EQ(sizeof(header.message_type), 1);
    EXPECT_EQ(sizeof(header.return_code), 1);  
    EXPECT_EQ(sizeof(header.tponlyheader), 4);  
}


TEST(TpHeader, CopyToBuffer)
{
    TpHeader header;
    unsigned char buffer[sizeof(header)];

    someIp::RequestId requestId;
    requestId.client_id = 0x494A;
    requestId.session_id = 0x4B4C;

    someIp::MessageId messageId;
    messageId._service_id = 0x4142;
    messageId._method_id = 0x4344;

    header.message_id = messageId;
    header.length = 0x45464748;
    header.request_id = requestId;
    header.protocol_version = 0x4D;
    header.interface_version = 0x4E;
    header.message_type = MessageType::REQUEST;
    header.return_code = ReturnCode::E_OK;
    header.tponlyheader.set_offset(0x0F0E0D0);
    header.tponlyheader.set_reserved(0x03);
    header.tponlyheader.set_msf(true);

    memcpy(buffer, &header, sizeof(header));

    // no additional padding
    EXPECT_EQ(sizeof(buffer), sizeof(header));

    EXPECT_EQ(std::memcmp(buffer, &header, sizeof(header)), 0);
}
