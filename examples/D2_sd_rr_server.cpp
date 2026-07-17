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

// D2 server, request response offered over SD
// POSIX loopback counterpart to D2_sd_rr_client. Mirrors STM32 SOMEIP_TESTCASE=2.

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "someIp/eSomeIP.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/service_discovery/SdService.hpp"
#include "someIp/service_discovery/Ipv4Option.hpp"
#include "someIp/service_discovery/LoadBalancingOption.hpp"
#include "common/constants.hpp"

using namespace someIp;

static std::mutex g_mtx;
static std::condition_variable g_cv;
static bool g_shutdown = false;

static std::string payload_str(PackageRx &p) {
  return std::string(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
}

int main() {
  printf("[D2] SD RR server, loopback\n");

  eSomeIP someIp;
  IpAddr server_ip = IpAddr::from_string(example::SERVER_IP);
  someIp.init(server_ip);
  someIp.listen_to_port(example::SERVER_PORT);

  auto echo = [&someIp](PackageRx &&p) {
    std::string bytes = payload_str(p);
    printf("[D2] Request: %s\n", bytes.c_str());
    someIp.send_response(std::move(p), bytes);
  };
  auto shutdown = [](PackageRx &&p) {
    printf("[D2] Shutdown: %s\n", payload_str(p).c_str());
    {
      std::lock_guard<std::mutex> l(g_mtx);
      g_shutdown = true;
    }
    g_cv.notify_one();
  };

  service *svc = someIp.add_service(example::SERVICE_ID);
  svc->register_method(Method(example::RPC_METHOD_ID, echo));
  svc->register_method(Method(example::SHUTDOWN_METHOD_ID, shutdown));

  // offer the service over SD
  sd::sd_service_info info{example::SERVICE_ID, example::INSTANCE_ID,
                           example::MAJOR_VERSION, example::MINOR_VERSION};
  info._src_port = example::SERVER_PORT;
  info._weight = 10;
  info._priority = 5;
  auto sd_service = std::make_shared<sd::sd_service>(info);

  someIp.enable_sd(server_ip);
  someIp.register_sd_service(sd_service);

  auto ld = std::make_shared<sd::load_balancing_option>(info._weight, info._priority, true);
  auto ep = std::make_shared<sd::ipv4_option>(server_ip, example::SERVER_PORT);
  sd::OptionVec opts;
  opts.push_back(ld);
  opts.push_back(ep);
  sd_service->update_options(opts);
  someIp.offer_service(sd_service, server_ip, info._src_port);

  printf("[D2] Offering service 0x%04X via SD on %s:%u\n", example::SERVICE_ID,
         example::SERVER_IP, example::SERVER_PORT);
  {
    std::unique_lock<std::mutex> l(g_mtx);
    g_cv.wait(l, [] { return g_shutdown; });
  }
  someIp.stop_all_services();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  printf("[D2] Done\n");
  return 0;
}
