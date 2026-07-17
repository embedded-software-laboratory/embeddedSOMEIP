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

// D3 server, field and eventgroup offered over SD on UDP
// POSIX loopback counterpart to D3_field_client. Mirrors STM32 SOMEIP_TESTCASE=3.

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/service_discovery/SdService.hpp"
#include "someIp/service_discovery/SdPool.hpp"
#include "someIp/service_discovery/Ipv4Option.hpp"
#include "someIp/service_discovery/LoadBalancingOption.hpp"
#include "common/constants.hpp"

using namespace someIp;

static std::mutex g_mtx;
static std::condition_variable g_cv;
static bool g_shutdown = false;

static sd::SdServiceInfo svc_info() {
  return sd::SdServiceInfo{example::SERVICE_ID, example::INSTANCE_ID,
                           example::MAJOR_VERSION, example::MINOR_VERSION};
}
static std::string payload_str(PackageRx &p) {
  return std::string(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
}

int main() {
  printf("[D3] Field/event server (SD, UDP), loopback\n");

  ESomeIp someIp;
  IpAddr server_ip = IpAddr::from_string(example::SERVER_IP);
  someIp.init(server_ip);
  someIp.set_transport_mode(TransportKind::UDP);
  someIp.listen_to_port(example::SERVER_PORT);

  auto field_get = [&someIp](PackageRx &&p) {
    auto v = someIp.get_event_payload(svc_info(), example::FIELD_EVENT_ID);
    printf("[D3] GET -> %.*s\n", static_cast<int>(v.size()),
           reinterpret_cast<const char *>(v.data()));
    someIp.send_response(std::move(p),
        std::string_view(reinterpret_cast<const char *>(v.data()), v.size()));
  };
  auto field_set = [&someIp](PackageRx &&p) {
    bool wants_response = (p.header._message_type == MessageType::REQUEST);
    std::string bytes = payload_str(p);
    if (bytes.size() > 64) bytes.resize(64);  // MAX_EVENT_PAYLOAD
    printf("[D3] SET %s\n", bytes.c_str());
    someIp.set_event_payload(svc_info(), example::FIELD_EVENT_ID, bytes);  // notifies subscribers
    if (bytes == "shutdown") {
      {
        std::lock_guard<std::mutex> l(g_mtx);
        g_shutdown = true;
      }
      g_cv.notify_one();
    }
    if (wants_response) someIp.send_response(std::move(p), bytes);
  };

  Service *svc = someIp.add_service(example::SERVICE_ID);
  svc->register_method(Method(example::FIELD_GET_ID, field_get));
  svc->register_method(Method(example::FIELD_SET_ID, field_set));

  sd::SdServiceInfo info = svc_info();
  info._src_port = example::SERVER_PORT;
  info._weight = 10;
  info._priority = 5;
  auto sd_service = sd::pool_make_service<sd::SdService>(info);

  someIp.enable_sd(server_ip);
  someIp.register_sd_service(sd_service);
  sd_service->register_event(example::FIELD_EVENT_ID, example::EVENTGROUP_ID, true,
                             TransportKind::UDP);

  auto ld = sd::pool_make<sd::LoadBalancingOption>(info._priority, info._weight, true);
  auto ep = sd::pool_make<sd::Ipv4Option>(server_ip, example::SERVER_PORT,
                                          sd::SdTransportProtocol::UDP, false);
  sd::OptionVec opts;
  opts.push_back(ld);
  opts.push_back(ep);
  sd_service->update_options(opts);
  someIp.offer_service(sd_service, server_ip, info._src_port);

  // initial value so a fresh subscriber gets notified on the Ack
  someIp.set_event_payload(svc_info(), example::FIELD_EVENT_ID, "initial-value");

  printf("[D3] Offering field service 0x%04X via SD\n", example::SERVICE_ID);
  {
    std::unique_lock<std::mutex> l(g_mtx);
    g_cv.wait(l, [] { return g_shutdown; });
  }
  someIp.stop_all_services();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  printf("[D3] Done\n");
  return 0;
}
