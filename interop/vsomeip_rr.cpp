#include <vsomeip/vsomeip.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr vsomeip::service_t SERVICE_ID = 0x7777;
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
constexpr vsomeip::major_version_t MAJOR_VERSION = 1;
constexpr vsomeip::minor_version_t MINOR_VERSION = 0;
constexpr vsomeip::method_t METHOD_ID = 0x0421;

struct Options { std::string role; int count = 10; int interval_ms = 300; int size = 64; };

std::atomic<bool> g_stop{false};
std::atomic<bool> g_registered{false};
std::atomic<bool> g_available{false};

int run_server(const Options &o) {
  (void)o;
  auto app = vsomeip::runtime::get()->create_application("vsomeip_interop");
  if (!app->init()) { std::fprintf(stderr, "vsomeip init failed\n"); return 1; }
  std::atomic<int> handled{0};

  app->register_state_handler([](vsomeip::state_type_e s) {
    g_registered.store(s == vsomeip::state_type_e::ST_REGISTERED);
  });
  // Answer each REQUEST with a RESPONSE echoing the request payload.
  app->register_message_handler(
      SERVICE_ID, INSTANCE_ID, METHOD_ID,
      [app, &handled](const std::shared_ptr<vsomeip::message> &req) {
        auto pl = req->get_payload();
        std::string in(reinterpret_cast<const char *>(pl->get_data()), pl->get_length());
        int n = handled.fetch_add(1) + 1;
        std::printf("[vsomeip][rr-server] req #%d \"%s\" -> responding\n", n, in.substr(0, 24).c_str());
        std::fflush(stdout);
        auto resp = vsomeip::runtime::get()->create_response(req);
        auto out = vsomeip::runtime::get()->create_payload();
        std::string reply = "vsomeip-reply:" + in.substr(0, 24);
        out->set_data(reinterpret_cast<const vsomeip::byte_t *>(reply.data()), static_cast<uint32_t>(reply.size()));
        resp->set_payload(out);
        app->send(resp);
      });

  std::thread io([app]() { app->start(); });
  while (!g_registered.load() && !g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
  app->offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
  std::printf("[vsomeip][rr-server] offering service 0x%04X method 0x%04X\n", SERVICE_ID, METHOD_ID);
  std::fflush(stdout);

  while (!g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  app->stop_offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
  app->stop();
  if (io.joinable()) io.join();
  std::printf("[vsomeip][rr-server] done, handled %d request(s)\n", handled.load());
  return 0;
}

int run_client(const Options &o) {
  auto app = vsomeip::runtime::get()->create_application("vsomeip_interop");
  if (!app->init()) { std::fprintf(stderr, "vsomeip init failed\n"); return 1; }
  std::atomic<int> responses{0};

  app->register_state_handler([](vsomeip::state_type_e s) {
    g_registered.store(s == vsomeip::state_type_e::ST_REGISTERED);
  });
  app->register_message_handler(
      SERVICE_ID, INSTANCE_ID, METHOD_ID,
      [&responses](const std::shared_ptr<vsomeip::message> &m) {
        auto pl = m->get_payload();
        std::string s(reinterpret_cast<const char *>(pl->get_data()), pl->get_length());
        int n = responses.fetch_add(1) + 1;
        std::printf("[vsomeip][rr-client] resp #%d: \"%s\"\n", n, s.substr(0, 32).c_str());
        std::fflush(stdout);
      });
  app->register_availability_handler(
      SERVICE_ID, INSTANCE_ID,
      [](vsomeip::service_t s, vsomeip::instance_t i, bool avail) {
        std::printf("[vsomeip][rr-client] service 0x%04X.0x%04X %s\n", s, i, avail ? "AVAILABLE" : "unavailable");
        std::fflush(stdout);
        g_available.store(avail);
      });

  std::thread io([app]() { app->start(); });
  while (!g_registered.load() && !g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
  app->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
  std::printf("[vsomeip][rr-client] requesting service 0x%04X ...\n", SERVICE_ID);
  std::fflush(stdout);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!g_available.load() && !g_stop.load() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  for (int i = 0; i < o.count && !g_stop.load(); ++i) {
    auto req = vsomeip::runtime::get()->create_request(false /* udp */);
    req->set_service(SERVICE_ID);
    req->set_instance(INSTANCE_ID);
    req->set_method(METHOD_ID);
    req->set_interface_version(MAJOR_VERSION);
    auto pl = vsomeip::runtime::get()->create_payload();
    std::string payload = "vsomeip-req:" + std::to_string(i);
    pl->set_data(reinterpret_cast<const vsomeip::byte_t *>(payload.data()), static_cast<uint32_t>(payload.size()));
    req->set_payload(pl);
    app->send(req);
    std::this_thread::sleep_for(std::chrono::milliseconds(o.interval_ms));
  }

  const auto rdeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (responses.load() < o.count && std::chrono::steady_clock::now() < rdeadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  std::printf("[vsomeip][rr-client] done, received %d/%d response(s)\n", responses.load(), o.count);
  app->release_service(SERVICE_ID, INSTANCE_ID);
  app->stop();
  if (io.joinable()) io.join();
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
    std::printf("vsomeip request-response interop tool\nUsage: %s --role server|client [--count N]\n", argv[0]);
    return 0;
  }
  return o.role == "server" ? run_server(o) : run_client(o);
}
