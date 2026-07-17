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

#include "someIp/communication/LinuxUdpTransport.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <algorithm>

namespace someIp {

IpAddr host_default_ipv4() {
  // default to INADDR_ANY (0.0.0.0) if no non-loopback interface is present
  IpAddr result = IpAddr::from_u32(0);
  ifaddrs *ptr_ifs = nullptr;
  if (::getifaddrs(&ptr_ifs) != 0) return result;
  for (ifaddrs *ptr_a = ptr_ifs; ptr_a; ptr_a = ptr_a->ifa_next) {
    if (!ptr_a->ifa_addr || ptr_a->ifa_addr->sa_family != AF_INET) continue;
    if ((ptr_a->ifa_flags & IFF_LOOPBACK) || !(ptr_a->ifa_flags & IFF_UP)) continue;
    auto *ptr_sin = reinterpret_cast<sockaddr_in *>(ptr_a->ifa_addr);
    // s_addr is network byte order, IpAddr is host byte order
    result = IpAddr::from_u32(ntohl(ptr_sin->sin_addr.s_addr));
    break;
  }
  ::freeifaddrs(ptr_ifs);
  return result;
}

LinuxUdpTransport::~LinuxUdpTransport() {
  m_rejoin_running.store(false);
  if (m_rejoin_thread.joinable()) {
    m_rejoin_thread.join();
  }
  // on macOS shutdown() does not unblock recvfrom, so poke each listener
  int wake_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  for (auto &ref_c : m_conns) {
    if (ref_c->fd >= 0) {
      ref_c->running.store(false);
      ::shutdown(ref_c->fd, SHUT_RDWR);
      if (wake_fd >= 0) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ref_c->port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::sendto(wake_fd, "", 0, 0, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
      }
      if (ref_c->thread.joinable()) {
        ref_c->thread.join();
      }
      ::close(ref_c->fd);
      ref_c->fd = -1;
    }
  }
  if (wake_fd >= 0) {
    ::close(wake_fd);
  }
}

void LinuxUdpTransport::init(const IpEndpoint &ref_local) { m_local = ref_local; }

void LinuxUdpTransport::set_rx_callback(RxCallback cb) { m_rx = std::move(cb); }

LinuxUdpTransport::Conn *LinuxUdpTransport::ensure_port(uint16_t port) {
  std::lock_guard<std::mutex> lock(m_mtx);
  for (auto &ref_c : m_conns) {
    if (ref_c->port == port) {
      return ref_c.get();
    }
  }

  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return nullptr;
  }

  // enlarge socket buffers to absorb microbursts
  int buf_size = 4 * 1024 * 1024;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

  // bounded blocking so recv_loop rechecks running if the wakeup is lost
  timeval rx_timeout{};
  rx_timeout.tv_usec = 100 * 1000; // 100 ms
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rx_timeout, sizeof(rx_timeout));

  // allow multiple binds to the SD multicast port
  int reuse = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
  // macOS also needs SO_REUSEPORT to share the SD multicast port
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return nullptr;
  }

  auto conn = std::make_unique<Conn>();
  conn->fd = fd;
  conn->port = port;
  conn->running.store(true);
  Conn *ptr_raw = conn.get();
  m_conns.push_back(std::move(conn));
  ptr_raw->thread = std::thread([this, ptr_raw]() { this->recv_loop(ptr_raw); });
  return ptr_raw;
}

void LinuxUdpTransport::recv_loop(Conn *ptr_c) {
  std::vector<uint8_t> recv_buf(65535);
  while (ptr_c->running.load()) {
    sockaddr_in src{};
    socklen_t srclen = sizeof(src);
    ssize_t n = ::recvfrom(ptr_c->fd, recv_buf.data(), recv_buf.size(), 0,
                           reinterpret_cast<sockaddr *>(&src), &srclen);
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      break; // socket shut down or fatal error
    }
    if (n == 0 || !m_rx) {
      continue;
    }
    if (src.sin_family != AF_INET) {
      continue;
    }

    RxPacket pkt;
    pkt.source = IpEndpoint(ntohl(src.sin_addr.s_addr), ntohs(src.sin_port));
    pkt.buffer = make_vector_buffer(std::vector<uint8_t>(recv_buf.data(), recv_buf.data() + n));
    m_rx(std::move(pkt));
  }
}

bool LinuxUdpTransport::open_listen_port(uint16_t port) {
  return ensure_port(port) != nullptr;
}

bool LinuxUdpTransport::join_multicast_group(const IpEndpoint &ref_group) {
  ip_mreq mreq{};
  mreq.imr_multiaddr.s_addr = htonl(ref_group.v4);
  mreq.imr_interface.s_addr = m_local.v4 != 0 ? htonl(m_local.v4) : INADDR_ANY;

  std::lock_guard<std::mutex> lock(m_mtx);
  bool any = false;
  for (auto &ref_c : m_conns) {
    if (ref_c->fd < 0) {
      continue;
    }
    if (m_local.v4 != 0) {
      in_addr ifaddr{};
      ifaddr.s_addr = htonl(m_local.v4);
      ::setsockopt(ref_c->fd, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr, sizeof(ifaddr));
    }
    if (::setsockopt(ref_c->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == 0) {
      any = true;
    }
  }
  if (any) {
    // remember group so rejoin_loop can refresh the membership
    if (std::find(m_groups.begin(), m_groups.end(), ref_group.v4) == m_groups.end()) {
      m_groups.push_back(ref_group.v4);
    }
    if (!m_rejoin_running.exchange(true)) {
      m_rejoin_thread = std::thread([this]() { this->rejoin_loop(); });
    }
  } else {
    // a silent join failure means SD never receives offers, so warn loudly
    char gbuf[16], ibuf[16];
    IpAddr::from_u32(ref_group.v4).to_string(gbuf, sizeof(gbuf));
    IpAddr::from_u32(m_local.v4).to_string(ibuf, sizeof(ibuf));
    std::fprintf(stderr,
        "[someip] WARNING: multicast join for %s via interface %s failed on all "
        "sockets; Service Discovery will receive no offers. Pass the local IP of "
        "this host on the server's subnet (e.g. --client-ip=<that IP>).\n",
        gbuf, m_local.v4 ? ibuf : "INADDR_ANY");
  }
  return any;
}

void LinuxUdpTransport::rejoin_loop() {
  using namespace std::chrono;
  // stay under vsomeip's ~1.1s multicast watchdog so our port never ages out
  constexpr auto kInterval = milliseconds(900);
  auto next = steady_clock::now() + kInterval;
  while (m_rejoin_running.load()) {
    // sleep in short slices so shutdown is prompt
    if (steady_clock::now() < next) {
      std::this_thread::sleep_for(milliseconds(50));
      continue;
    }
    next = steady_clock::now() + kInterval;

    std::lock_guard<std::mutex> lock(m_mtx);
    for (uint32_t g : m_groups) {
      ip_mreq mreq{};
      mreq.imr_multiaddr.s_addr = htonl(g);
      mreq.imr_interface.s_addr = m_local.v4 != 0 ? htonl(m_local.v4) : INADDR_ANY;
      for (auto &ref_c : m_conns) {
        if (ref_c->fd < 0) {
          continue;
        }
        ::setsockopt(ref_c->fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        ::setsockopt(ref_c->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
      }
    }
  }
}

bool LinuxUdpTransport::send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t src_port) {
  if (!buf) {
    return false;
  }
  // 0 means an ephemeral source socket
  Conn *ptr_c = src_port != 0 ? ensure_port(src_port) : ensure_port(0);
  if (!ptr_c || ptr_c->fd < 0) {
    return false;
  }

  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(ref_dest.port);
  to.sin_addr.s_addr = htonl(ref_dest.v4);

  const uint8_t *ptr_data = buf.data();
  const size_t len = buf.size();
  if (!ptr_data || len == 0) {
    return false;
  }
  ssize_t sent = ::sendto(ptr_c->fd, ptr_data, len, 0, reinterpret_cast<sockaddr *>(&to), sizeof(to));
  return sent == static_cast<ssize_t>(len);
}

} // namespace someIp
