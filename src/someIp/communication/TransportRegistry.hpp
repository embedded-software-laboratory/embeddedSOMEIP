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

#ifndef SOMEIP_COMMUNICATION_TRANSPORTREGISTRY_HPP
#define SOMEIP_COMMUNICATION_TRANSPORTREGISTRY_HPP

#include <new>
#include <type_traits>
#include <utility>

#include "someIp/communication/ITransport.hpp"
#include "someIp/communication/Endpoint.hpp"
#include "someIp/config/communication_config.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp {

// holds the UDP and TCP transports, routes by TransportKind
class TransportRegistry {
public:
  TransportRegistry() = default;
  TransportRegistry(const TransportRegistry &) = delete;
  TransportRegistry &operator=(const TransportRegistry &) = delete;

  // UDP, UDP_TP and DEFAULT all route to this one
  template <class T, class... Args>
  void emplace_udp(Args &&...args) { m_udp.emplace<T>(std::forward<Args>(args)...); }

  template <class T, class... Args>
  void emplace_tcp(Args &&...args) { m_tcp.emplace<T>(std::forward<Args>(args)...); }

  ITransport *get(TransportKind k) const {
    return (k == TransportKind::TCP) ? m_tcp.ptr : m_udp.ptr;
  }

  ITransport *udp() const { return m_udp.ptr; }
  ITransport *tcp() const { return m_tcp.ptr; }

  void set_rx_callback(RxCallback cb) {
    if (m_udp.ptr) m_udp.ptr->set_rx_callback(cb);
    if (m_tcp.ptr) m_tcp.ptr->set_rx_callback(cb);
  }

  void init_all(const IpEndpoint &ref_local) {
    if (m_udp.ptr) m_udp.ptr->init(ref_local);
    if (m_tcp.ptr) m_tcp.ptr->init(ref_local);
  }

private:
  // transport held inline, no heap
  struct Slot {
    alignas(std::max_align_t) unsigned char storage[config::TRANSPORT_INPLACE_SIZE];
    ITransport *ptr = nullptr;
    void (*destroy_)(void *) = nullptr;

    Slot() = default;
    Slot(const Slot &) = delete;
    Slot &operator=(const Slot &) = delete;
    ~Slot() { reset(); }

    void reset() {
      if (destroy_) { destroy_(storage); destroy_ = nullptr; }
      ptr = nullptr;
    }
    template <class T, class... Args>
    void emplace(Args &&...args) {
      static_assert(sizeof(T) <= config::TRANSPORT_INPLACE_SIZE,
                    "transport backend too large (raise TRANSPORT_INPLACE_SIZE)");
      reset();
      ptr = ::new (storage) T(std::forward<Args>(args)...);
      destroy_ = [](void *ptr_s) { static_cast<T *>(ptr_s)->~T(); };
    }
  };

  Slot m_udp, m_tcp;
};

} // namespace someIp

#endif // SOMEIP_COMMUNICATION_TRANSPORTREGISTRY_HPP
