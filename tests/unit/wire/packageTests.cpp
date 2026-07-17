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
#include <iomanip>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/Package.hpp"

TEST(Package, CalculatingLengthOfPackageWithoutPayload) {
  someIp::Package package;

  // header is 8 bytes [PRS_SOMEIP_00042]
  EXPECT_EQ(package.calculateLength(), 8);
}

TEST(Package, CalculatingLengthOfPackageWithPayload) {
  std::string payload = "hello world";

  someIp::Package package = someIp::Package(0, 0, payload, someIp::IpAddr{}, 0);
  EXPECT_EQ(package.calculateLength(), payload.size() + 8);
}

TEST(Package, SizeofNullptr) {
  EXPECT_EQ(sizeof(nullptr), 8);
}

TEST(Package, ConstructorWithServiceAndMethodId){
  uint16_t serviceId = 1;
  uint16_t methodId = 2;
  std::string payload = "Payload";
  uint16_t port = 8080;
  someIp::Package package = someIp::Package(serviceId, methodId, payload, someIp::IpAddr{}, port);

  EXPECT_EQ(package.header._message_id._service_id, serviceId);
  EXPECT_EQ(package.header._message_id._method_id, methodId);
  EXPECT_EQ(package.getDestinationPort(), port);

  auto p = package.serialize();

  EXPECT_EQ(p.data()[1], 1); // service id
  EXPECT_EQ(p.data()[3], 2); // method id
}

TEST(Package, ConstructorWithMessageId){
  uint16_t serviceId = 1;
  uint16_t methodId = 2;
  someIp::MessageId messageId;
  messageId._service_id = serviceId;
  messageId._method_id = methodId;
  std::string payload = "Payload";
  uint16_t port = 8080;
  someIp::Package package = someIp::Package(messageId, payload, someIp::IpAddr{}, port);

  EXPECT_EQ(package.header._message_id._service_id, serviceId);
  EXPECT_EQ(package.header._message_id._method_id, methodId);
  EXPECT_EQ(package.getDestinationPort(), port);
}


// TEST(Package, BigEndian) {
//   someIp::Header header;

//   someIp::RequestId requestId;
//   requestId.client_id = 0x0003;
//   requestId.session_id = 0x0004;

//   someIp::MessageId messageId;
//   messageId.service_id = 0x0001;
//   messageId.method_id = 0x0002;

//   header.message_id = messageId;
//   header.length = 0x45464748;
//   header.request_id = requestId;
//   header.protocol_version = 0x4D;
//   header.interface_version = 0x4E;
//   header.message_type = MessageType::REQUEST;
//   header.return_code = ReturnCode::E_OK;

//   someIp::Package package;
//   package.header = header;
//   auto pbuffer = package.serialize();

//   // Print the payload content of pbuffer
//   std::cout << "Payload content: ";
//   for (size_t i = 0; i < pbuffer.get()->len; ++i) {
//     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)((uint8_t*)pbuffer.get()->payload)[i] << " ";
//   }
//   std::cout << std::dec << std::endl;

//   // Check if each field is in big endian
//   EXPECT_EQ(((uint8_t*)pbuffer.get()->payload)[1], 1);
//   EXPECT_EQ(((uint8_t*)pbuffer.get()->payload)[3], 2);
//   EXPECT_EQ(((uint8_t*)pbuffer.get()->payload)[7], 8);
//   EXPECT_EQ(((uint8_t*)pbuffer.get()->payload)[9], 3);
//   EXPECT_EQ(((uint8_t*)pbuffer.get()->payload)[11], 4);
// }
