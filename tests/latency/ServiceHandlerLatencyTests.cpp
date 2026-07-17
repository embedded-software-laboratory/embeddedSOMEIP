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
#include <memory>

#include "utils/SuppressOutput.hpp"
#include "utils/Statistics.hpp"
#include "utils/ProgessBar.hpp"
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp
{

  class ServiceHandlerLatencyTests : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      Singleton<ServiceHandler>::reset_instance();
      serviceHandler = ServiceHandler::get_instance();
      latencies = std::make_shared<std::vector<double>>();
    }

    // services are now stored by value in fixed inline storage
    static constexpr size_t maxSize = config::MAX_SERVICES;
    ServiceHandler* serviceHandler = nullptr;

    std::shared_ptr<std::vector<double>> latencies;

    std::shared_ptr<SuppressOutput> suppress = std::make_shared<SuppressOutput>();
  };

  TEST_F(ServiceHandlerLatencyTests, RegisterServiceSuccessfully)
  {
    const int NUMBER_OF_RUNS = static_cast<int>(maxSize);

    for (int i = 0; i < NUMBER_OF_RUNS; i++)
    {
      auto startTime = std::chrono::high_resolution_clock::now();

      ASSERT_TRUE(serviceHandler->register_service(Service(static_cast<uint16_t>(i))));
      ASSERT_NE(serviceHandler->find_service_by_id(static_cast<uint16_t>(i)), nullptr);

      auto endTime = std::chrono::high_resolution_clock::now();

      std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
      latencies->push_back(elapsed.count());
    }

    std::string result = Statistics::statistic_output(latencies);
    suppress->PrintOnce([result, NUMBER_OF_RUNS]
                        {
    printf("%d runs -> ", NUMBER_OF_RUNS);
    printf("%s", result.c_str()); });
  }

  TEST_F(ServiceHandlerLatencyTests, FindServiceById)
  {
    const int NUMBER_OF_RUNS = static_cast<int>(maxSize);

    suppress->PrintOnce([NUMBER_OF_RUNS]
                        { printf("Creating the maximum possible number of Services (%d)...\n", NUMBER_OF_RUNS); });

    ProgressBar barCreate = ProgressBar(NUMBER_OF_RUNS, suppress);
    for (int i = 0; i < NUMBER_OF_RUNS; i++)
    {
      serviceHandler->register_service(Service(static_cast<uint16_t>(i)));
      barCreate.show(i);
    }
    barCreate.complete();
    suppress->PrintOnce([]
                        { printf("Measuring time to find a Service...\n"); });

    ProgressBar barFind = ProgressBar(NUMBER_OF_RUNS, suppress);
    for (int i = 0; i < NUMBER_OF_RUNS; i++)
    {
      auto startTime = std::chrono::high_resolution_clock::now();

      ASSERT_EQ(serviceHandler->find_service_by_id(static_cast<uint16_t>(i))->get_id(), i);

      auto endTime = std::chrono::high_resolution_clock::now();

      std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
      latencies->push_back(elapsed.count());
      barFind.show(i);
    }
    barFind.complete();

    std::string result = Statistics::statistic_output(latencies);
    suppress->PrintOnce([result]
                        { printf("%s", result.c_str()); });
  }

} // namespace someIp
