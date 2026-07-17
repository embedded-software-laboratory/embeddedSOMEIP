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
#include <string.h>

#include "someIp/structs/Header.hpp"

// Demonstrate some basic assertions.
TEST(Header, SizeInByte) {
  someIp::Header header;

  // Check byte size of each header field
  EXPECT_EQ(sizeof(header), 16);
  EXPECT_EQ(sizeof(header._message_id), 4);
  EXPECT_EQ(sizeof(header._message_id._method_id), 2);
  EXPECT_EQ(sizeof(header._message_id._service_id), 2);
  EXPECT_EQ(sizeof(header._length), 4);
  EXPECT_EQ(sizeof(header._request_id), 4);
  EXPECT_EQ(sizeof(header._request_id.client_id), 2);
  EXPECT_EQ(sizeof(header._request_id.session_id), 2);
  EXPECT_EQ(sizeof(header._protocol_version), 1);
  EXPECT_EQ(sizeof(header._interface_version), 1);
  EXPECT_EQ(sizeof(header._message_type), 1);
  EXPECT_EQ(sizeof(header._return_code), 1);
}

TEST(Header, CopyToBuffer){
  someIp::Header header;
  unsigned char buffer[sizeof(header)];

  someIp::RequestId requestId;
  requestId.client_id = 0x494A;
  requestId.session_id = 0x4B4C;

	someIp::MessageId messageId;
	messageId._service_id = 0x4142;
	messageId._method_id = 0x4344;

  header._message_id = messageId;
  header._length = 0x45464748;
  header._request_id = requestId;
  header._protocol_version = 0x4D;
  header._interface_version = 0x4E;
  header._message_type = MessageType::REQUEST;
  header._return_code = ReturnCode::E_OK;

  memcpy(buffer, &header, sizeof(header));

  // Check byte size (no additional padding)
  EXPECT_EQ(sizeof(buffer), sizeof(header));

  // Check if buffer contains the header
  EXPECT_EQ(memcmp(buffer, &header, sizeof(header)), 0);
}
