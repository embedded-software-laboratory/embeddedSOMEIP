
#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/config/communication_config.hpp"
#include "someIp/communication/TransportRegistry.hpp"
#include "someIp/communication/LinuxUdpTransport.hpp" // host_default_ipv4()
#include "someIp/net/IpAddress.hpp"
#include "someIp/service_discovery/SdPool.hpp"        // pool_make / pool_make_service
#include "someIp/service_discovery/SdService.hpp"
#include "someIp/service_discovery/SdClientService.hpp"
#include "someIp/service_discovery/Ipv4Option.hpp"
#include "someIp/service_discovery/LoadBalancingOption.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace someIp;

namespace {

constexpr uint16_t SERVICE_ID = 0x7777;
constexpr uint16_t INSTANCE_ID = 0x0001;
// must match embeddedSOMEIP's INTERFACE_VERSION config, vsomeip sends it
constexpr uint8_t MAJOR_VERSION = 1;
constexpr uint32_t MINOR_VERSION = 0;
constexpr uint16_t EVENTGROUP_ID = 0x0001;
constexpr uint16_t EVENT_ID = 0x8001;

ESomeIp::TransportPopulator udp_populator() {
  return [](TransportRegistry &reg) { reg.emplace_udp<LinuxUdpTransport>(); };
}

struct Options {
  std::string role;            // "pub" or "sub"
  std::string local_ip;        // dotted IPv4; empty -> auto-detect
  int base_port = 40000;       // local unicast/listen base port
  int count = 50;              // samples to send (pub)
  int interval_ms = 200;       // publish interval (pub)
  int size = 64;               // payload size (pub)
};

bool parse_args(int argc, char **argv, Options &o, std::string &err) {
  for (int i = 1; i < argc; ++i) {
    std::string a(argv[i]);
    auto next = [&](const char *name) -> const char * {
      if (i + 1 >= argc) { err = std::string("missing value for ") + name; return nullptr; }
      return argv[++i];
    };
    if (a == "--role") { const char *v = next("--role"); if (!v) return false; o.role = v; }
    else if (a == "--local-ip") { const char *v = next("--local-ip"); if (!v) return false; o.local_ip = v; }
    else if (a == "--base-port") { const char *v = next("--base-port"); if (!v) return false; o.base_port = std::atoi(v); }
    else if (a == "--count") { const char *v = next("--count"); if (!v) return false; o.count = std::atoi(v); }
    else if (a == "--interval-ms") { const char *v = next("--interval-ms"); if (!v) return false; o.interval_ms = std::atoi(v); }
    else if (a == "--size") { const char *v = next("--size"); if (!v) return false; o.size = std::atoi(v); }
    else if (a == "-h" || a == "--help") { o.role = "help"; return true; }
    else { err = "unknown arg: " + a; return false; }
  }
  if (o.role != "pub" && o.role != "sub" && o.role != "help") {
    err = "--role must be pub or sub";
    return false;
  }
  if (o.size < 4) o.size = 4;
  return true;
}

void usage(const char *prog) {
  std::printf(
      "embeddedSOMEIP interop tool\n"
      "Usage: %s --role pub|sub [--local-ip A.B.C.D] [--base-port N]\n"
      "          [--count N] [--interval-ms N] [--size N]\n"
      "Fixed IDs: service=0x7777 instance=0x0001 eventgroup=0x0001 event=0x8001 (UDP)\n",
      prog);
}

IpAddr resolve_local_ip(const Options &o) {
  IpAddr ip;
  if (!o.local_ip.empty() && IpAddr::from_string(o.local_ip.c_str(), ip)) return ip;
  return host_default_ipv4();
}

std::string rx_to_string(PackageRx &p) {
  return std::string(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
}

std::atomic<bool> g_stop{false};

int run_publisher(const Options &o, IpAddr local_ip) {
  ESomeIp api(udp_populator());
  api.init(local_ip);
  api.set_transport_mode(TransportKind::UDP);
  const uint16_t actual_port = api.listen_to_port(o.base_port);

  sd::SdServiceInfo info{SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION};
  info._src_port = actual_port;
  info._weight = 10;
  info._priority = 5;
  auto sd_service = sd::pool_make_service<sd::SdService>(info);
  sd_service->register_event(EVENT_ID, EVENTGROUP_ID, false, TransportKind::UDP);

  sd::ServiceVec sd_services;
  api.enable_sd(local_ip, sd_services);
  api.register_sd_service(sd_service);

  sd::OptionVec options;
  options.push_back(sd::pool_make<sd::LoadBalancingOption>(info._weight, info._priority, true));
  options.push_back(sd::pool_make<sd::Ipv4Option>(local_ip, actual_port, sd::SdTransportProtocol::UDP));
  sd_service->update_options(options);
  api.offer_service(sd_service, local_ip, actual_port);

  std::printf("[esomeip][pub] offering service 0x%04X event 0x%04X on %s:%u, waiting for subscribers...\n",
              SERVICE_ID, EVENT_ID, local_ip.c_str(), actual_port);
  std::fflush(stdout);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!g_stop.load() && std::chrono::steady_clock::now() < deadline) {
    if (!sd_service->get_subscribers_of_eventgroup(EVENTGROUP_ID).empty()) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const size_t subs = sd_service->get_subscribers_of_eventgroup(EVENTGROUP_ID).size();
  std::printf("[esomeip][pub] subscribers=%zu; sending %d notifications\n", subs, o.count);
  std::fflush(stdout);

  std::vector<uint8_t> payload(static_cast<size_t>(o.size), 0);
  for (int i = 0; i < o.count && !g_stop.load(); ++i) {
    std::string head = "esomeip:" + std::to_string(i);
    std::memset(payload.data(), 'a', payload.size());
    std::memcpy(payload.data(), head.data(), std::min(head.size(), payload.size()));

    auto subscribers = sd_service->get_subscribers_of_eventgroup(EVENTGROUP_ID);
    for (const auto &subscriber : subscribers) {
      const auto &ep = subscriber._option;
      Package pkg(SERVICE_ID, EVENT_ID, payload.data(), payload.size(), ep._ip, ep._port);
      pkg.header._message_type = MessageType::NOTIFICATION;
      // vsomeip's event socket filters on the provider endpoint
      pkg.setSrcPort(actual_port);
      api.fire_and_forget(std::move(pkg), TransportKind::UDP);
    }
    std::printf("[esomeip][pub] sent seq=%d to %zu subscriber(s)\n", i, subscribers.size());
    std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(o.interval_ms));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  api.stop_all_services();
  std::printf("[esomeip][pub] done\n");
  return 0;
}

int run_subscriber(const Options &o, IpAddr local_ip) {
  ESomeIp api(udp_populator());
  api.init(local_ip);
  api.set_transport_mode(TransportKind::UDP);

  std::atomic<int> received{0};
  api.register_event_handler(SERVICE_ID, EVENT_ID, [&received](PackageRx &&p) {
    std::string s = rx_to_string(p);
    int n = received.fetch_add(1) + 1;
    std::string view = s.substr(0, 24);
    std::printf("[esomeip][sub] recv #%d (%zu bytes): \"%s\"\n", n, s.size(), view.c_str());
    std::fflush(stdout);
  });

  const uint16_t udp_port = api.listen_to_port(o.base_port);

  sd::ServiceVec sd_services;
  api.enable_sd(local_ip, sd_services);

  sd::SdServiceInfo info{SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION};
  auto client = sd::pool_make<sd::SdClientService>(info);
  api.register_client_service(client);

  std::printf("[esomeip][sub] discovering service 0x%04X on %s:%u ...\n",
              SERVICE_ID, local_ip.c_str(), udp_port);
  std::fflush(stdout);

  const auto discover_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (client->get_phase() != sd::SdClientPhase::MAIN &&
         client->get_phase() != sd::SdClientPhase::STOPPED &&
         !g_stop.load() &&
         std::chrono::steady_clock::now() < discover_deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  std::printf("[esomeip][sub] discovered=%d; subscribing\n",
              client->get_phase() == sd::SdClientPhase::MAIN);
  std::fflush(stdout);

  auto do_subscribe = [&]() {
    sd::OptionVec options;
    options.push_back(sd::pool_make<sd::Ipv4Option>(
        local_ip, udp_port, sd::SdTransportProtocol::UDP, false));
    api.subscribe(sd::SdEventgroupInfo{SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, EVENTGROUP_ID}, options);
  };
  do_subscribe();

  // re-subscribe until data flows, recovers a lost single-shot SUBSCRIBE
  auto last_resub = std::chrono::steady_clock::now();
  while (!g_stop.load()) {
    if (received.load() == 0 &&
        std::chrono::steady_clock::now() - last_resub >= std::chrono::seconds(1)) {
      do_subscribe();
      last_resub = std::chrono::steady_clock::now();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  api.unsubscribe_all();
  std::printf("[esomeip][sub] done, received %d notification(s)\n", received.load());
  return 0;
}

} // namespace

#include <csignal>

int main(int argc, char **argv) {
  Options o;
  std::string err;
  if (!parse_args(argc, argv, o, err)) {
    std::fprintf(stderr, "arg error: %s\n", err.c_str());
    usage(argv[0]);
    return 2;
  }
  if (o.role == "help") { usage(argv[0]); return 0; }

  std::signal(SIGINT, [](int) { g_stop.store(true); });
  std::signal(SIGTERM, [](int) { g_stop.store(true); });

  IpAddr local_ip = resolve_local_ip(o);
  if (o.role == "pub") return run_publisher(o, local_ip);
  return run_subscriber(o, local_ip);
}
