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

#ifndef EXAMPLES_COMMON_RAW_UDP_HPP
#define EXAMPLES_COMMON_RAW_UDP_HPP

// POSIX UDP helper for the plain-UDP baseline clients B1 and B3.
// Bypasses SOME/IP to establish the latency floor T1 and T3 compare against.
// send_once matches the contract latency::run_sweep expects.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "common/latency.hpp"  // latency::clock

namespace rawudp {

// the 1000 ms default timeout matches the SOME/IP clients' Waiter::wait_ms
class Endpoint {
 public:
  Endpoint() = default;
  ~Endpoint() {
    if (fd_ >= 0) ::close(fd_);
  }
  Endpoint(const Endpoint &) = delete;
  Endpoint &operator=(const Endpoint &) = delete;

  bool ok() const { return fd_ >= 0; }

  // binds locally so replies land on the expected interface and port
  bool open(const std::string &client_ip, uint16_t client_port,
            const std::string &server_ip, uint16_t server_port,
            int timeout_ms = 1000) {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
      std::perror("socket");
      return false;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(client_port);
    if (::inet_pton(AF_INET, client_ip.c_str(), &local.sin_addr) != 1) {
      std::fprintf(stderr, "invalid --client-ip: %s\n", client_ip.c_str());
      return false;
    }
    if (::bind(fd_, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0) {
      std::perror("bind");
      return false;
    }

    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    server_ = sockaddr_in{};
    server_.sin_family = AF_INET;
    server_.sin_port = htons(server_port);
    if (::inet_pton(AF_INET, server_ip.c_str(), &server_.sin_addr) != 1) {
      std::fprintf(stderr, "invalid --server-ip: %s\n", server_ip.c_str());
      return false;
    }
    return true;
  }

  // returns the RTT in microseconds, or -1.0 when the reply drops
  double send_once(const std::string &payload) {
    auto t0 = latency::clock::now();
    ssize_t sent = ::sendto(fd_, payload.data(), payload.size(), 0,
                            reinterpret_cast<sockaddr *>(&server_), sizeof(server_));
    if (sent < 0) return -1.0;

    char buf[2048];
    for (;;) {
      ssize_t got = ::recvfrom(fd_, buf, sizeof(buf), 0, nullptr, nullptr);
      if (got >= 0) {
        auto t1 = latency::clock::now();
        return std::chrono::duration<double, std::micro>(t1 - t0).count();
      }
      if (errno == EINTR) continue;                    // retry on signal
      if (errno == EAGAIN || errno == EWOULDBLOCK) return -1.0;  // timed out
      return -1.0;                                     // any other error is a drop
    }
  }

 private:
  int fd_ = -1;
  sockaddr_in server_{};
};

}  // namespace rawudp

#endif  // EXAMPLES_COMMON_RAW_UDP_HPP
