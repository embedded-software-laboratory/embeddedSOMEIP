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
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/config/StackConfig.hpp"

#include "utils/SuppressOutput.hpp"

namespace someIp
{

  class ServiceHandlerTest : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      Singleton<ServiceHandler>::reset_instance();
      serviceHandler = ServiceHandler::get_instance();
    }

    ServiceHandler* serviceHandler = nullptr;

    SuppressOutput suppress;
  };

  TEST_F(ServiceHandlerTest, RegisterServiceSuccessfully)
  {
    EXPECT_TRUE(serviceHandler->register_service(Service(1)));
    Service* found = serviceHandler->find_service_by_id(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->get_id(), 1);
  }

  TEST_F(ServiceHandlerTest, RegisterServiceFailsIfAlreadyRegistered)
  {
    EXPECT_TRUE(serviceHandler->register_service(Service(1)));
    EXPECT_FALSE(serviceHandler->register_service(Service(1)));
  }

  TEST_F(ServiceHandlerTest, RegisterServiceFailsIfStorageIsFull)
  {
    for (size_t i = 0; i < config::MAX_SERVICES; ++i)
    {
      EXPECT_TRUE(serviceHandler->register_service(Service(static_cast<uint16_t>(i))));
    }
    EXPECT_FALSE(serviceHandler->register_service(Service(static_cast<uint16_t>(config::MAX_SERVICES))));
  }

  TEST_F(ServiceHandlerTest, FindServiceById)
  {
    serviceHandler->register_service(Service(1));
    ASSERT_NE(serviceHandler->find_service_by_id(1), nullptr);
    EXPECT_EQ(serviceHandler->find_service_by_id(2), nullptr);
  }

  TEST_F(ServiceHandlerTest, DeregisterServiceSuccessfully)
  {
    serviceHandler->register_service(Service(1));
    serviceHandler->deregister_service(1);
    EXPECT_EQ(serviceHandler->find_service_by_id(1), nullptr);
  }

  TEST_F(ServiceHandlerTest, DeregisterServiceFailsIfNotRegistered)
  {
    serviceHandler->register_service(Service(1));
    serviceHandler->deregister_service(2);
    ASSERT_NE(serviceHandler->find_service_by_id(1), nullptr);
  }

} // namespace someIp

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
