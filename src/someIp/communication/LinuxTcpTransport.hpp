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

#ifndef SOMEIP_COMMUNICATION_LINUXTCPTRANSPORT_HPP
#define SOMEIP_COMMUNICATION_LINUXTCPTRANSPORT_HPP

#include <cstdint>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#include "someIp/communication/ITransport.hpp"

namespace someIp {

// POSIX-sockets TCP transport, accept thread per port, recv thread per conn
class LinuxTcpTransport : public ITransport {
public:
  LinuxTcpTransport() = default;
  ~LinuxTcpTransport() override;

  void init(const IpEndpoint &ref_local) override;
  void set_rx_callback(RxCallback cb) override;
  bool open_listen_port(uint16_t port) override;
  bool join_multicast_group(const IpEndpoint &) override { return false; }
  bool send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t src_port = 0) override;

  ConnectionId connect(const IpEndpoint &ref_dest) override;
  bool is_connected(const IpEndpoint &ref_dest) override;
  ConnectionId find_connection(const IpEndpoint &ref_dest) override;
  bool send_on(ConnectionId conn, BufferPtr buf) override;

  TransportKind kind() const override { return TransportKind::TCP; }

private:
  // listen socket (accept thread) or established stream (recv thread)
  struct Conn {
    int fd = -1;
    IpEndpoint peer;            // remote endpoint, unused for listeners
    bool is_listen = false;
    std::thread thread;
    std::atomic<bool> running{false};
  };

  Conn *ensure_client(const IpEndpoint &ref_dest);
  // registers the socket and starts its recv thread
  Conn *add_data_conn(int fd, const IpEndpoint &ref_peer);
  static bool write_all(int fd, const uint8_t *ptr_data, size_t len);
  void accept_loop(Conn *ptr_listener);
  void recv_loop(Conn *ptr_c);

  // heap is fine here, the embedded path uses LwipTcpTransport
  // unique_ptr keeps each Conn address stable for the detached threads
  std::vector<std::unique_ptr<Conn>> m_conns;
  std::mutex m_mtx;
  RxCallback m_rx;
  IpEndpoint m_local;
};

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_LINUXTCPTRANSPORT_HPP
