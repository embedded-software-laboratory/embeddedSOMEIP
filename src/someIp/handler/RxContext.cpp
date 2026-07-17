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

#include "someIp/handler/RxContext.hpp"
#include "RxContext.hpp"

namespace someIp {

// --- Event Handlers ---
void RxContext::register_event_handler(uint16_t serviceId, uint16_t eventId,
                                       Callback callback) {
  uint32_t key = make_event_key(serviceId, eventId);
  os::Guard _g__event_mutex(m_event_mutex);
  m_event_handlers[key] = std::move(callback);
}

void RxContext::deregister_event_handler(uint16_t serviceId, uint16_t eventId) {
  uint32_t key = make_event_key(serviceId, eventId);
  os::Guard _g__event_mutex(m_event_mutex);
  m_event_handlers.erase(key);
}

Callback RxContext::find_event_handler(uint16_t serviceId, uint16_t eventId) const {
  uint32_t key = make_event_key(serviceId, eventId);
  os::Guard _g__event_mutex(m_event_mutex);
  auto ptr_it = m_event_handlers.find(key);
  Callback result = (ptr_it != m_event_handlers.end()) ? ptr_it->second : Callback{};
  return result;
}

} // namespace someIp
