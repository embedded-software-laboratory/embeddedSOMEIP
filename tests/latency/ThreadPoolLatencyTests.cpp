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
#include <gmock/gmock.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <unistd.h> // usleep

#include "someIp/net/IpAddress.hpp"
#include "someIp/ThreadPool.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/ESomeIp.hpp"
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/pattern/Singleton.hpp"
#include "someIp/utils/Lock.hpp"
#include "mocks/MockCallback.hpp"
#include "utils/SuppressOutput.hpp"
#include "utils/ServiceCreationHelper.hpp"
#include "utils/Statistics.hpp"
#include "utils/ProgessBar.hpp"

using namespace someIp;

class ThreadPoolLatency : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Singleton<ServiceHandler>::reset_instance();
    // suppress->AllowOutput(); // uncomment to see SOME/IP output
  }
  std::shared_ptr<SuppressOutput> suppress = std::make_shared<SuppressOutput>();
};

TEST_F(ThreadPoolLatency, MeasureTheTimeItTakesToInvokeRxHandler)
{
  const int NUMBER_OF_RUNS = 10;
  auto latencies = std::make_shared<std::vector<double>>();

  MockCallback mockCallback;
  std::chrono::high_resolution_clock::time_point callTime;
  std::atomic<bool> wasCalled = false;

  std::recursive_mutex mutex;

  EXPECT_CALL(mockCallback, Call(testing::_))
      .WillRepeatedly(testing::Invoke([&callTime, &wasCalled, &mutex]()
                                      {
      Lock lock(mutex);
      callTime = std::chrono::high_resolution_clock::now();
      wasCalled = true; }));


  auto service = ServiceCreationHelper::create_service_with_one_method(0, 0, [&mockCallback](someIp::PackageRx p)
                                                                       { mockCallback.SafeCall(std::move(p)); });
  auto serviceHandler = someIp::ServiceHandler::get_instance();
  serviceHandler->register_service(std::move(service));
  
  someIp::TransportRegistry transports;
  someIp::ThreadPool threadPool = someIp::ThreadPool(transports, nullptr);
  threadPool.start_threads();

  suppress->PrintOnce([]
                      { printf("Measuring ThreadPool Latency (number or runs: %d)...\n", NUMBER_OF_RUNS); });
  ProgressBar progress = ProgressBar(NUMBER_OF_RUNS, suppress);
  std::chrono::high_resolution_clock::time_point startTime;
  for (int i = 0; i < NUMBER_OF_RUNS; i++)
  {
    someIp::IpAddr ip;
    someIp::Package package = someIp::Package(0, 0, "", ip, 0);
    someIp::PackageRx packageRx = someIp::PackageRx(package.serialize(), ip, 0);

    startTime = std::chrono::high_resolution_clock::now();
    ASSERT_TRUE(threadPool.add_buffer_to_queue(std::move(packageRx)));

    int breakCounter = 0;
    while (!wasCalled)
    {
      usleep(5);
      breakCounter++;
      if (breakCounter > 2000)
      {
        EXPECT_TRUE(false) << "Callback was not called within the expected time.";
        break;
      }
    }
    wasCalled = false;
    std::chrono::duration<double, std::milli> elapsed = callTime - startTime;
    latencies->push_back(elapsed.count());

    suppress->PrintOnce([elapsed](){
      printf("Elapsed time: %.3f ms\n", elapsed.count()); });

    EXPECT_GT(elapsed.count(), 0); // confirms the handler was invoked
    progress.show(i);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  progress.complete();

  std::string result = Statistics::statistic_output(latencies);

  suppress->PrintOnce([result]()
                      { printf("%s", result.c_str()); });

  EXPECT_TRUE(threadPool.stop_threads());
}
