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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mocks/MockCallback.hpp"
#include "utils/ServiceCreationHelper.hpp"

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/PackageRxHandleResult.hpp"
#include "someIp/storages/ThreadSafeCircularBuffer.hpp"
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/handler/RxHandler.hpp"
#include "someIp/config/config_linux.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/structs/Package.hpp"

#include <iomanip>

#include "utils/SuppressOutput.hpp"

using namespace someIp;

class RxHandlerTests : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Singleton<ServiceHandler>::reset_instance();
  }

  // SuppressOutput suppress; // Remove to see Debug output of SOME/IP
};

TEST_F(RxHandlerTests, HandleSuccessfully)
{
  MockCallback mockCallback;
  EXPECT_CALL(mockCallback, Call(testing::_)).Times(1);


  uint16_t serviceId = 0;
  uint16_t methodId = 0;

  auto service = ServiceCreationHelper::create_service_with_one_method(serviceId, methodId, [&mockCallback](someIp::PackageRx &&p)
                                                                       { mockCallback.Call(std::move(p)); });

  auto serviceHandler = someIp::ServiceHandler::get_instance();
  serviceHandler->register_service(std::move(service));

  IpAddr ip = IpAddr::from_string("172.30.206.22");

  uint16_t port = 8080;

  std::string payload = "Hello World!";

  auto package = Package(serviceId, methodId, payload, ip, port);
  package.header._message_type = MessageType::REQUEST;

  auto buf = package.serialize();

  EXPECT_EQ(buf.size(), 16 + payload.length());

  printf("\n");
  for (size_t i = 0; i < buf.size(); i++)
  {
    printf("%02X ", buf.data()[i]);
  }
  printf("\n");
  auto rx = PackageRx(std::move(buf), ip, 0);

  handler::RxHandler handler = handler::RxHandler(nullptr, serviceHandler);
  auto result = handler.handle_rx_package(std::move(rx));

  ASSERT_EQ(result.code, ReturnCode::E_OK);
}
