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
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp
{

  class ServiceTests : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      serv = Service(10);
    }

    static constexpr size_t maxSize = config::MAX_METHODS_PER_SERVICE;
    Service serv{10};
  };

  TEST_F(ServiceTests, RegisterMethod_Success)
  {
    EXPECT_TRUE(serv.register_method(Method(1)));
  }

  TEST_F(ServiceTests, RegisterMethod_Fail_DuplicateId)
  {
    serv.register_method(Method(1));
    EXPECT_FALSE(serv.register_method(Method(1)));
  }

  TEST_F(ServiceTests, RegisterMethod_Fail_MaxSize)
  {
    for (size_t i = 0; i < maxSize; ++i)
    {
      EXPECT_TRUE(serv.register_method(Method(static_cast<uint16_t>(i))));
    }
    EXPECT_FALSE(serv.register_method(Method(static_cast<uint16_t>(maxSize))));
  }

  TEST_F(ServiceTests, FindMethodById_Success)
  {
    serv.register_method(Method(1));
    Method *foundMethod = serv.find_method_by_id(1);
    ASSERT_NE(foundMethod, nullptr);
    EXPECT_EQ(foundMethod->get_id(), 1);
  }

  TEST_F(ServiceTests, FindMethodById_Fail)
  {
    EXPECT_EQ(serv.find_method_by_id(1), nullptr);
  }

  TEST_F(ServiceTests, FindMethodById_OrderIndependent)
  {
    serv.register_method(Method(2));
    serv.register_method(Method(1));
    Method *foundMethod1 = serv.find_method_by_id(1);
    Method *foundMethod2 = serv.find_method_by_id(2);
    ASSERT_NE(foundMethod1, nullptr);
    ASSERT_NE(foundMethod2, nullptr);
    EXPECT_EQ(foundMethod1->get_id(), 1);
    EXPECT_EQ(foundMethod2->get_id(), 2);
  }

}
