#include <vsomeip/vsomeip.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr vsomeip::service_t SERVICE_ID = 0x7777;
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
// must match embeddedSOMEIP's INTERFACE_VERSION config, sent as interface-version
constexpr vsomeip::major_version_t MAJOR_VERSION = 1;
constexpr vsomeip::minor_version_t MINOR_VERSION = 0;
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0001;
constexpr vsomeip::event_t EVENT_ID = 0x8001;

struct Options {
  std::string role;
  int count = 50;
  int interval_ms = 200;
  int size = 64;
};

bool parse_args(int argc, char **argv, Options &o, std::string &err) {
  for (int i = 1; i < argc; ++i) {
    std::string a(argv[i]);
    auto next = [&](const char *name) -> const char * {
      if (i + 1 >= argc) { err = std::string("missing value for ") + name; return nullptr; }
      return argv[++i];
    };
    if (a == "--role") { const char *v = next("--role"); if (!v) return false; o.role = v; }
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
  if (o.size < 1) o.size = 1;
  return true;
}

void usage(const char *prog) {
  std::printf(
      "vsomeip interop tool\n"
      "Usage: VSOMEIP_CONFIGURATION=interop/vsomeip.json %s --role pub|sub\n"
      "          [--count N] [--interval-ms N] [--size N]\n"
      "Fixed IDs: service=0x7777 instance=0x0001 eventgroup=0x0001 event=0x8001\n", prog);
}

std::atomic<bool> g_stop{false};
std::atomic<bool> g_registered{false};

int run_publisher(const Options &o) {
  auto app = vsomeip::runtime::get()->create_application("vsomeip_interop");
  if (!app->init()) { std::fprintf(stderr, "vsomeip init failed\n"); return 1; }
  app->register_state_handler([](vsomeip::state_type_e s) {
    g_registered.store(s == vsomeip::state_type_e::ST_REGISTERED);
  });

  std::atomic<int> subscribers{0};
  app->register_subscription_handler(
      SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID,
      [&subscribers](vsomeip::client_t, vsomeip::uid_t, vsomeip::gid_t, bool subscribed) -> bool {
        subscribers.fetch_add(subscribed ? 1 : -1);
        std::printf("[vsomeip][pub] subscription %s (now %d)\n",
                    subscribed ? "added" : "removed", subscribers.load());
        std::fflush(stdout);
        return true;
      });

  std::thread io([app]() { app->start(); });
  while (!g_registered.load() && !g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(2));

  std::set<vsomeip::eventgroup_t> egs{EVENTGROUP_ID};
  app->offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, egs,
                   vsomeip::event_type_e::ET_EVENT, std::chrono::milliseconds::zero(),
                   false, true, nullptr, vsomeip::reliability_type_e::RT_UNRELIABLE);
  app->offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
  std::printf("[vsomeip][pub] offered service 0x%04X event 0x%04X; waiting for subscribers...\n",
              SERVICE_ID, EVENT_ID);
  std::fflush(stdout);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (subscribers.load() <= 0 && !g_stop.load() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  auto pl = vsomeip::runtime::get()->create_payload();
  std::vector<vsomeip::byte_t> data(static_cast<size_t>(o.size), 'b');
  for (int i = 0; i < o.count && !g_stop.load(); ++i) {
    std::string head = "vsomeip:" + std::to_string(i);
    std::memset(data.data(), 'b', data.size());
    std::memcpy(data.data(), head.data(), std::min(head.size(), data.size()));
    pl->set_data(data.data(), static_cast<uint32_t>(data.size()));
    app->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, pl, true);
    std::printf("[vsomeip][pub] sent seq=%d\n", i);
    std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(o.interval_ms));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  app->stop_offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
  app->stop();
  if (io.joinable()) io.join();
  std::printf("[vsomeip][pub] done\n");
  return 0;
}

int run_subscriber(const Options &o) {
  (void)o;
  auto app = vsomeip::runtime::get()->create_application("vsomeip_interop");
  if (!app->init()) { std::fprintf(stderr, "vsomeip init failed\n"); return 1; }

  std::atomic<int> received{0};
  app->register_state_handler([](vsomeip::state_type_e s) {
    g_registered.store(s == vsomeip::state_type_e::ST_REGISTERED);
  });
  app->register_availability_handler(
      SERVICE_ID, INSTANCE_ID,
      [](vsomeip::service_t s, vsomeip::instance_t i, bool avail) {
        std::printf("[vsomeip][sub] service 0x%04X.0x%04X %s\n", s, i, avail ? "AVAILABLE" : "unavailable");
        std::fflush(stdout);
      });
  app->register_message_handler(
      SERVICE_ID, INSTANCE_ID, EVENT_ID,
      [&received](const std::shared_ptr<vsomeip::message> &m) {
        auto pl = m->get_payload();
        std::string s(reinterpret_cast<const char *>(pl->get_data()), pl->get_length());
        int n = received.fetch_add(1) + 1;
        std::printf("[vsomeip][sub] recv #%d (%u bytes): \"%s\"\n",
                    n, pl->get_length(), s.substr(0, 24).c_str());
        std::fflush(stdout);
      });

  std::thread io([app]() { app->start(); });
  while (!g_registered.load() && !g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(2));

  std::set<vsomeip::eventgroup_t> egs{EVENTGROUP_ID};
  app->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
  app->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, egs,
                     vsomeip::event_type_e::ET_EVENT, vsomeip::reliability_type_e::RT_UNRELIABLE);
  app->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);
  std::printf("[vsomeip][sub] requested + subscribed service 0x%04X event 0x%04X\n", SERVICE_ID, EVENT_ID);
  std::fflush(stdout);

  while (!g_stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds(50));

  app->unsubscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
  app->release_event(SERVICE_ID, INSTANCE_ID, EVENT_ID);
  app->release_service(SERVICE_ID, INSTANCE_ID);
  app->stop();
  if (io.joinable()) io.join();
  std::printf("[vsomeip][sub] done, received %d notification(s)\n", received.load());
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

  if (o.role == "pub") return run_publisher(o);
  return run_subscriber(o);
}
