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

// D4 client, fire and forget echo without SD on UDP
// Message count comes from argv[1].

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "someIp/eSomeIP.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "common/constants.hpp"

using namespace someIp;

static std::atomic<int> g_echoes{0};

int main(int argc, char *argv[]) {
  int n = (argc > 1) ? std::atoi(argv[1]) : 3;
  if (n <= 0) n = 1;
  printf("[D4] FF client, sending %d message(s) to %s:%u\n", n, example::SERVER_IP,
         example::SERVER_PORT);

  eSomeIP someIp;
  IpAddr client_ip = IpAddr::from_string(example::CLIENT_IP);
  someIp.init(client_ip);
  someIp.set_transport_mode(TransportKind::UDP);
  someIp.listen_to_port(example::CLIENT_PORT);

  // the server's bounced-back message arrives as a REQUEST_NO_RETURN here
  service *svc = someIp.add_service(example::SERVICE_ID);
  svc->register_method(Method(example::OWD_METHOD_ID, [](PackageRx &&p) {
    std::string bytes(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
    printf("[D4] Echo: %s\n", bytes.c_str());
    g_echoes.fetch_add(1, std::memory_order_relaxed);
  }));

  IpAddr server_ip = IpAddr::from_string(example::SERVER_IP);
  for (int i = 0; i < n; ++i) {
    std::string payload = "ping " + std::to_string(i + 1);
    Package p(example::SERVICE_ID, example::OWD_METHOD_ID, payload, server_ip, example::SERVER_PORT);
    p.header._message_type = MessageType::REQUEST_NO_RETURN;
    someIp.fire_and_forget(std::move(p), TransportKind::UDP);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  int waited = 0;
  while (g_echoes.load(std::memory_order_relaxed) < n && waited < 2000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    waited += 20;
  }

  Package bye(example::SERVICE_ID, example::SHUTDOWN_METHOD_ID, "bye", server_ip, example::SERVER_PORT);
  bye.header._message_type = MessageType::REQUEST_NO_RETURN;
  someIp.fire_and_forget(std::move(bye), TransportKind::UDP);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  printf("[D4] Done (%d echoes)\n", g_echoes.load());
  return 0;
}
