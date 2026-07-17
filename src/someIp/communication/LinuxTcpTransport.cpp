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

#include "someIp/communication/LinuxTcpTransport.hpp"

#include "someIp/config/StackConfig.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace someIp {

// macOS lacks MSG_NOSIGNAL, it uses SO_NOSIGPIPE per socket instead
#ifdef MSG_NOSIGNAL
static constexpr int kSendFlags = MSG_NOSIGNAL;
#else
static constexpr int kSendFlags = 0;
#endif

static void disable_sigpipe(int fd) {
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  int one = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
  (void)fd;
#endif
}

LinuxTcpTransport::~LinuxTcpTransport() {
  // stop threads before joining so accept_loop cannot append new conns
  {
    std::lock_guard<std::mutex> lock(m_mtx);
    for (auto &ref_c : m_conns) {
      ref_c->running.store(false);
      if (ref_c->fd >= 0) {
        ::shutdown(ref_c->fd, SHUT_RDWR);
      }
    }
  }
  for (auto &ref_c : m_conns) {
    if (ref_c->thread.joinable()) {
      ref_c->thread.join();
    }
    if (ref_c->fd >= 0) {
      ::close(ref_c->fd);
      ref_c->fd = -1;
    }
  }
}

void LinuxTcpTransport::init(const IpEndpoint &ref_local) { m_local = ref_local; }

void LinuxTcpTransport::set_rx_callback(RxCallback cb) { m_rx = std::move(cb); }

bool LinuxTcpTransport::write_all(int fd, const uint8_t *ptr_data, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t n = ::send(fd, ptr_data + off, len - off, kSendFlags);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    off += static_cast<size_t>(n);
  }
  return true;
}

LinuxTcpTransport::Conn *LinuxTcpTransport::add_data_conn(int fd, const IpEndpoint &ref_peer) {
  auto conn = std::make_unique<Conn>();
  conn->fd = fd;
  conn->peer = ref_peer;
  conn->is_listen = false;
  conn->running.store(true);
  Conn *ptr_raw = conn.get();
  m_conns.push_back(std::move(conn));
  ptr_raw->thread = std::thread([this, ptr_raw]() { this->recv_loop(ptr_raw); });
  return ptr_raw;
}

LinuxTcpTransport::Conn *LinuxTcpTransport::ensure_client(const IpEndpoint &ref_dest) {
  std::lock_guard<std::mutex> lock(m_mtx);
  for (auto &ref_c : m_conns) {
    if (!ref_c->is_listen && ref_c->fd >= 0 && ref_c->peer == ref_dest) {
      return ref_c.get();
    }
  }

  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return nullptr;
  }
  disable_sigpipe(fd);
  int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(ref_dest.port);
  to.sin_addr.s_addr = htonl(ref_dest.v4);
  if (::connect(fd, reinterpret_cast<sockaddr *>(&to), sizeof(to)) < 0) {
    ::close(fd);
    return nullptr;
  }
  return add_data_conn(fd, ref_dest);
}

void LinuxTcpTransport::accept_loop(Conn *ptr_listener) {
  while (ptr_listener->running.load()) {
    sockaddr_in src{};
    socklen_t srclen = sizeof(src);
    int client = ::accept(ptr_listener->fd, reinterpret_cast<sockaddr *>(&src), &srclen);
    if (client < 0) {
      if (errno == EINTR) continue;
      break;
    }
    disable_sigpipe(client);
    int one = 1;
    ::setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    IpEndpoint peer(ntohl(src.sin_addr.s_addr), ntohs(src.sin_port));
    std::lock_guard<std::mutex> lock(m_mtx);
    if (!ptr_listener->running.load()) {
      ::close(client);
      break;
    }
    add_data_conn(client, peer);
  }
}

void LinuxTcpTransport::recv_loop(Conn *ptr_c) {
  std::vector<uint8_t> acc;       // stream reassembly accumulator
  std::vector<uint8_t> chunk(65535);
  while (ptr_c->running.load()) {
    ssize_t n = ::recv(ptr_c->fd, chunk.data(), chunk.size(), 0);
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
      break;
    }
    if (n == 0) {
      break; // peer closed
    }
    acc.insert(acc.end(), chunk.data(), chunk.data() + n);

    // length field at offset 4 counts bytes after it, so total = 8 + length
    for (;;) {
      if (acc.size() < 8) break;
      uint32_t length = (static_cast<uint32_t>(acc[4]) << 24) |
                        (static_cast<uint32_t>(acc[5]) << 16) |
                        (static_cast<uint32_t>(acc[6]) << 8) |
                        (static_cast<uint32_t>(acc[7]));
      if (length < 8 || (8u + length) > config::MAX_TCP_RX_MESSAGE) {
        // drop the connection on a malformed or oversized length
        ptr_c->running.store(false);
        break;
      }
      const size_t total = 8u + length;
      if (acc.size() < total) break;

      if (m_rx) {
        RxPacket pkt;
        pkt.source = ptr_c->peer;
        pkt.buffer = make_vector_buffer(std::vector<uint8_t>(acc.begin(), acc.begin() + total));
        m_rx(std::move(pkt));
      }
      acc.erase(acc.begin(), acc.begin() + total);
    }
  }
}

bool LinuxTcpTransport::open_listen_port(uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }
  int reuse = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 ||
      ::listen(fd, 16) < 0) {
    ::close(fd);
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mtx);
  auto conn = std::make_unique<Conn>();
  conn->fd = fd;
  conn->is_listen = true;
  conn->running.store(true);
  Conn *ptr_raw = conn.get();
  m_conns.push_back(std::move(conn));
  ptr_raw->thread = std::thread([this, ptr_raw]() { this->accept_loop(ptr_raw); });
  return true;
}

bool LinuxTcpTransport::send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t /*src_port*/) {
  if (!buf) {
    return false;
  }
  const uint8_t *ptr_data = buf.data();
  const size_t len = buf.size();
  if (!ptr_data || len == 0) {
    return false;
  }
  Conn *ptr_c = ensure_client(ref_dest);
  if (!ptr_c || ptr_c->fd < 0) {
    return false;
  }
  return write_all(ptr_c->fd, ptr_data, len);
}

ConnectionId LinuxTcpTransport::connect(const IpEndpoint &ref_dest) {
  Conn *ptr_c = ensure_client(ref_dest);
  return ptr_c ? ConnectionId(TransportKind::TCP, ptr_c) : ConnectionId();
}

bool LinuxTcpTransport::is_connected(const IpEndpoint &ref_dest) {
  std::lock_guard<std::mutex> lock(m_mtx);
  for (auto &ref_c : m_conns) {
    if (!ref_c->is_listen && ref_c->fd >= 0 && ref_c->peer == ref_dest) {
      return true;
    }
  }
  return false;
}

ConnectionId LinuxTcpTransport::find_connection(const IpEndpoint &ref_dest) {
  std::lock_guard<std::mutex> lock(m_mtx);
  for (auto &ref_c : m_conns) {
    if (!ref_c->is_listen && ref_c->fd >= 0 && ref_c->peer == ref_dest) {
      return ConnectionId(TransportKind::TCP, ref_c.get());
    }
  }
  return ConnectionId();
}

bool LinuxTcpTransport::send_on(ConnectionId conn, BufferPtr buf) {
  if (conn.kind != TransportKind::TCP || conn.handle == nullptr || !buf) {
    return false;
  }
  const uint8_t *ptr_data = buf.data();
  const size_t len = buf.size();
  if (!ptr_data || len == 0) {
    return false;
  }
  auto *ptr_c = static_cast<Conn *>(conn.handle);
  if (ptr_c->fd < 0) {
    return false;
  }
  return write_all(ptr_c->fd, ptr_data, len);
}

} // namespace someIp
