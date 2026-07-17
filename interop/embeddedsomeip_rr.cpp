
#include "someIp/ESomeIp.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/structs/Method.hpp"
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
constexpr uint8_t MAJOR_VERSION = 1;   // carried in the SOME/IP interface-version field
constexpr uint32_t MINOR_VERSION = 0;
constexpr uint16_t METHOD_ID = 0x0421; // RPC method (not an event)

ESomeIp::TransportPopulator udp_populator() {
  return [](TransportRegistry &reg) { reg.emplace_udp<LinuxUdpTransport>(); };
}

struct Options {
  std::string role;
  std::string local_ip;
  int base_port = 40000;
  int count = 10;
  int interval_ms = 300;
  int size = 64;
};

std::atomic<bool> g_stop{false};

std::string rx_to_string(PackageRx &p) {
  return std::string(reinterpret_cast<const char *>(p.payload.data()), p.payload.size());
}

IpAddr resolve_local_ip(const Options &o) {
  IpAddr ip;
  if (!o.local_ip.empty() && IpAddr::from_string(o.local_ip.c_str(), ip)) return ip;
  return host_default_ipv4();
}

int run_server(const Options &o, IpAddr local_ip) {
  ESomeIp api(udp_populator());
  api.init(local_ip);
  api.set_transport_mode(TransportKind::UDP);
  const uint16_t port = api.listen_to_port(o.base_port);

  std::atomic<int> handled{0};

  sd::SdServiceInfo info{SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION};
  info._src_port = port;
  info._weight = 10;
  info._priority = 5;
  auto sd_service = sd::pool_make_service<sd::SdService>(info);
  sd::ServiceVec sd_services;
  api.enable_sd(local_ip, sd_services);
  sd::OptionVec options;
  options.push_back(sd::pool_make<sd::LoadBalancingOption>(info._weight, info._priority, true));
  options.push_back(sd::pool_make<sd::Ipv4Option>(local_ip, port, sd::SdTransportProtocol::UDP));
  sd_service->update_options(options);
  api.offer_service(sd_service, local_ip, port);

  auto cb = [&api, &handled](PackageRx &&p) {
    std::string req = rx_to_string(p);
    int n = handled.fetch_add(1) + 1;
    std::string reply = "esomeip-reply:" + req.substr(0, 24);
    std::printf("[esomeip][rr-server] req #%d \"%s\" -> responding\n", n, req.substr(0, 24).c_str());
    std::fflush(stdout);
    api.send_response(std::move(p), reply);
  };
  // no accepted-MessageType list any more, the dispatcher routes by id
  Service svc(SERVICE_ID);
  svc.register_method(Method(METHOD_ID, cb));
  api.register_service(std::move(svc));

  std::printf("[esomeip][rr-server] offering service 0x%04X method 0x%04X on %s:%u\n",
              SERVICE_ID, METHOD_ID, local_ip.c_str(), port);
  std::fflush(stdout);

  while (!g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::printf("[esomeip][rr-server] done, handled %d request(s)\n", handled.load());
  api.stop_all_services();
  return 0;
}

int run_client(const Options &o, IpAddr local_ip) {
  ESomeIp api(udp_populator());
  api.init(local_ip);
  api.set_transport_mode(TransportKind::UDP);
  api.listen_to_port(o.base_port);

  std::atomic<int> responses{0};

  sd::ServiceVec sd_services;
  api.enable_sd(local_ip, sd_services);
  sd::SdServiceInfo info{SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION};
  auto client = sd::pool_make<sd::SdClientService>(info);
  api.register_client_service(client);

  std::printf("[esomeip][rr-client] discovering service 0x%04X ...\n", SERVICE_ID);
  std::fflush(stdout);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (client->get_phase() != sd::SdClientPhase::MAIN &&
         client->get_phase() != sd::SdClientPhase::STOPPED &&
         !g_stop.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  auto wanted = api.find_service(info);
  if (wanted == nullptr) {
    std::printf("[esomeip][rr-client] ERROR: service not discovered\n");
    return 1;
  }
  sd::SdEndpointInfo ep = wanted->get_endpoint();
  IpAddr server_ip = ep._ip;
  uint16_t server_port = ep._port;
  std::printf("[esomeip][rr-client] discovered server at %s:%u; sending %d request(s)\n",
              server_ip.c_str(), server_port, o.count);
  std::fflush(stdout);

  auto on_response = [&responses](PackageRx &&p) {
    std::string s = rx_to_string(p);
    int n = responses.fetch_add(1) + 1;
    std::printf("[esomeip][rr-client] resp #%d: \"%s\"\n", n, s.substr(0, 32).c_str());
    std::fflush(stdout);
  };

  for (int i = 0; i < o.count && !g_stop.load(); ++i) {
    std::string payload = "esomeip-req:" + std::to_string(i);
    Package req(SERVICE_ID, METHOD_ID, payload, server_ip, server_port);
    req.header._message_type = MessageType::REQUEST;
    api.request_and_response(std::move(req), on_response, TransportKind::UDP);
    std::this_thread::sleep_for(std::chrono::milliseconds(o.interval_ms));
  }

  const auto rdeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (responses.load() < o.count && std::chrono::steady_clock::now() < rdeadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  std::printf("[esomeip][rr-client] done, received %d/%d response(s)\n", responses.load(), o.count);
  return 0;
}

bool parse_args(int argc, char **argv, Options &o, std::string &err) {
  for (int i = 1; i < argc; ++i) {
    std::string a(argv[i]);
    auto next = [&](const char *n) -> const char * {
      if (i + 1 >= argc) { err = std::string("missing value for ") + n; return nullptr; }
      return argv[++i];
    };
    if (a == "--role") { auto v = next("--role"); if (!v) return false; o.role = v; }
    else if (a == "--local-ip") { auto v = next("--local-ip"); if (!v) return false; o.local_ip = v; }
    else if (a == "--base-port") { auto v = next("--base-port"); if (!v) return false; o.base_port = std::atoi(v); }
    else if (a == "--count") { auto v = next("--count"); if (!v) return false; o.count = std::atoi(v); }
    else if (a == "--interval-ms") { auto v = next("--interval-ms"); if (!v) return false; o.interval_ms = std::atoi(v); }
    else if (a == "--size") { auto v = next("--size"); if (!v) return false; o.size = std::atoi(v); }
    else if (a == "-h" || a == "--help") { o.role = "help"; return true; }
    else { err = "unknown arg: " + a; return false; }
  }
  if (o.role != "server" && o.role != "client" && o.role != "help") { err = "--role must be server or client"; return false; }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  Options o;
  std::string err;
  if (!parse_args(argc, argv, o, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; }
  if (o.role == "help") {
    std::printf("embeddedSOMEIP request-response interop tool\n"
                "Usage: %s --role server|client [--local-ip A.B.C.D] [--base-port N] [--count N]\n", argv[0]);
    return 0;
  }
  IpAddr local_ip = resolve_local_ip(o);
  return o.role == "server" ? run_server(o, local_ip) : run_client(o, local_ip);
}
