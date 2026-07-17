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

// D4 server, fire and forget echo without SD on UDP
// Two one-way hops let a client estimate OWD without clock sync.
// POSIX loopback counterpart to D4_ff_client. Mirrors STM32 SOMEIP_TESTCASE=4.

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

static std::string payload_str(PackageRx &p) {
  return std::string(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
}

int main() {
  printf("[D4] Fire-and-forget echo server (no SD, UDP), loopback\n");

  eSomeIP someIp;
  IpAddr server_ip = IpAddr::from_string(example::SERVER_IP);
  someIp.init(server_ip);
  someIp.set_transport_mode(TransportKind::UDP);
  someIp.listen_to_port(example::SERVER_PORT);

  auto ff_echo = [&someIp](PackageRx &&p) {
    std::string bytes = payload_str(p);
    IpAddr src = p.get_source_ip();
    uint16_t src_port = p.get_source_port();
    printf("[D4] Bounce %zu B back to %s:%u\n", bytes.size(), src.c_str(), src_port);
    Package reply(example::SERVICE_ID, example::OWD_METHOD_ID, bytes, src, src_port);
    reply.header._message_type = MessageType::REQUEST_NO_RETURN;
    someIp.fire_and_forget(std::move(reply), TransportKind::UDP);
  };
  auto shutdown = [](PackageRx &&p) {
    printf("[D4] Shutdown: %s\n", payload_str(p).c_str());
    {
      std::lock_guard<std::mutex> l(g_mtx);
      g_shutdown = true;
    }
    g_cv.notify_one();
  };

  service *svc = someIp.add_service(example::SERVICE_ID);
  svc->register_method(Method(example::OWD_METHOD_ID, ff_echo));
  svc->register_method(Method(example::SHUTDOWN_METHOD_ID, shutdown));

  printf("[D4] Listening on %s:%u\n", example::SERVER_IP, example::SERVER_PORT);
  {
    std::unique_lock<std::mutex> l(g_mtx);
    g_cv.wait(l, [] { return g_shutdown; });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  printf("[D4] Done\n");
  return 0;
}
