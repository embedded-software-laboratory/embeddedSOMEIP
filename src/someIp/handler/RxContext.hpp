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

#ifndef RX_CONTEXT_HPP
#define RX_CONTEXT_HPP

#include "someIp/Types.hpp"
#include "someIp/os/Os.hpp"
#include "someIp/utils/StaticMap.hpp"
#include "someIp/config/StackConfig.hpp"

#include <cstdint>

namespace someIp {

// thread-safe registry mapping Service/Instance/Method IDs to callbacks
class RxContext {
public:
  RxContext() = default;
  ~RxContext() = default;

  void register_event_handler(uint16_t serviceId, uint16_t eventId,
                              Callback callback);

  void deregister_event_handler(uint16_t serviceId, uint16_t eventId);

  // returns a copy taken under the lock, stays valid across concurrent deregister
  Callback find_event_handler(uint16_t serviceId, uint16_t eventId) const;

private:
  // (ServiceId << 16 | EventId) -> Callback (stored by value, no heap)
  StaticMap<uint32_t, Callback, config::MAX_RX_EVENT_HANDLERS> m_event_handlers;
  mutable os::Mutex m_event_mutex;

  static uint32_t make_event_key(uint16_t serviceId, uint16_t eventId) {
    return (uint32_t(serviceId) << 16) | uint32_t(eventId);
  }
};

} // namespace someIp

#endif // RX_CONTEXT_HPP
