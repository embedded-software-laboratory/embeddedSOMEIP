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

#ifndef SOMEIP_COMMUNICATION_ITRANSPORT_HPP
#define SOMEIP_COMMUNICATION_ITRANSPORT_HPP

#include <cstdint>
#include <functional>

#include "someIp/communication/Endpoint.hpp"
#include "someIp/communication/ByteBuffer.hpp"
#include "someIp/config/communication_config.hpp"

namespace someIp {

// handle for a stream connection or listen socket
struct ConnectionId {
  TransportKind kind = TransportKind::DEFAULT;
  void *handle = nullptr;

  ConnectionId() = default;
  ConnectionId(TransportKind k, void *ptr_h) : kind(k), handle(ptr_h) {}

  explicit operator bool() const { return handle != nullptr; }
};

// conn is set only for stream transports
struct RxPacket {
  IpEndpoint source;
  BufferPtr buffer;
  ConnectionId conn;
};

// RX sink registered by the owner (eSomeIP)
using RxCallback = std::function<void(RxPacket &&)>;

// transport-neutral send/receive seam, backends translate to lwIP/POSIX internally
class ITransport {
public:
  virtual ~ITransport() = default;

  virtual void init(const IpEndpoint &ref_local) = 0;

  // single RX sink, fanned out by TransportRegistry
  virtual void set_rx_callback(RxCallback cb) = 0;

  virtual bool open_listen_port(uint16_t port) = 0;

  // no-op for stream transports
  virtual bool join_multicast_group(const IpEndpoint &ref_group) = 0;

  // src_port selects the source binding
  virtual bool send_to(const IpEndpoint &ref_dest, BufferPtr buf, uint16_t src_port = 0) = 0;

  // no-op for datagram transports
  virtual ConnectionId connect(const IpEndpoint &ref_dest) = 0;
  virtual bool is_connected(const IpEndpoint &ref_dest) = 0;
  virtual ConnectionId find_connection(const IpEndpoint &ref_dest) = 0;

  virtual bool send_on(ConnectionId conn, BufferPtr buf) = 0;

  // keys the registry
  virtual TransportKind kind() const = 0;
};

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_ITRANSPORT_HPP
