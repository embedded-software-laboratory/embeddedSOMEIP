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

// D1 server, direct request response without SD
// POSIX loopback counterpart to D1_rr_client. Same feature as STM32 SOMEIP_TESTCASE=1.

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "someIp/eSomeIP.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "common/constants.hpp"

using namespace someIp;

static std::mutex g_mtx;
static std::condition_variable g_cv;
static bool g_shutdown = false;
static int g_handled = 0;

static std::string payload_str(PackageRx &p) {
  return std::string(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
}

int main() {
  printf("[D1] RR server (no SD), loopback\n");

  eSomeIP someIp;
  IpAddr server_ip = IpAddr::from_string(example::SERVER_IP);
  someIp.init(server_ip);
  someIp.listen_to_port(example::SERVER_PORT);

  auto echo = [&someIp](PackageRx &&p) {
    ++g_handled;
    std::string bytes = payload_str(p);
    printf("[D1] Request: %s\n", bytes.c_str());
    someIp.send_response(std::move(p), bytes);
  };
  auto shutdown = [](PackageRx &&p) {
    printf("[D1] Shutdown: %s\n", payload_str(p).c_str());
    {
      std::lock_guard<std::mutex> l(g_mtx);
      g_shutdown = true;
    }
    g_cv.notify_one();
  };

  service *svc = someIp.add_service(example::SERVICE_ID);
  svc->register_method(Method(example::RPC_METHOD_ID, echo));
  svc->register_method(Method(example::SHUTDOWN_METHOD_ID, shutdown));

  printf("[D1] Listening on %s:%u\n", example::SERVER_IP, example::SERVER_PORT);
  {
    std::unique_lock<std::mutex> l(g_mtx);
    g_cv.wait(l, [] { return g_shutdown; });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  printf("[D1] Done, handled %d request(s)\n", g_handled);
  return 0;
}
