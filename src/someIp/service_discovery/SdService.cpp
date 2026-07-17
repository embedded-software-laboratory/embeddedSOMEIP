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

#include "SdService.hpp"
#include "someIp/utils/PoolPtr.hpp"

#include <algorithm>
#include <cstdint>

#include "someIp/os/Os.hpp"
#include "someIp/logging/BaseLogger.hpp"

namespace {
constexpr char SD_SERVICE_TAG[] = "SD_SERVICE";
using SD_SERVICE_LOGGER = BaseLogger<SD_SERVICE_TAG>;
} // namespace

namespace someIp
{
    namespace sd
    {
        SdService::SdService(const SdServiceInfo &ref_service_info) : Service(ref_service_info._service_id), m_info(ref_service_info) {}
        SdService::SdService(const SdServiceInfo &ref_service_info,
                               const OptionVec &ref_options) : Service(ref_service_info._service_id),
                                                                                      m_info(ref_service_info),
                                                                                      m_options(ref_options) {}
        SdService::SdService(const SdServiceInfo &ref_service_info,
                               someIp::PoolPtr<TimerEvent> sprt_timer,
                               const OptionVec &ref_options) : Service(ref_service_info._service_id),
                                                                                      m_info(ref_service_info),
                                                                                      m_sprt_ttl_timer(sprt_timer),
                                                                                      m_options(ref_options) {}
        SdService::SdService(const SdServiceInfo &ref_service_info, const SdEndpointInfo &ref_endpoint) : Service(ref_service_info._service_id),
                                                                                                        m_info(ref_service_info),
                                                                                                        m_endpoint(ref_endpoint) {}

        bool SdService::remove_subscriber(uint16_t eventgroup_id, const SdSubscriberInfo &ref_subscriber)
        {
            os::Guard lock(m_subscribers_mtx);
            // bind by reference so removal mutates the stored list, not a copy (M-6)
            auto &ref_subscribers = m_eventgroup_subscribers[eventgroup_id];
            auto ptr_it = std::remove_if(ref_subscribers.begin(), ref_subscribers.end(), [&ref_subscriber](const auto &ref_endpoint)
                                     { return ref_subscriber == ref_endpoint; });
            if (ptr_it == ref_subscribers.end())
            {
                return false;
            }
            ref_subscribers.erase(ptr_it, ref_subscribers.end());
            return true;
        }

        bool SdService::add_subscriber(uint16_t eventgroup_id, const SdSubscriberInfo &ref_subscriber)
        {
            os::Guard lock(m_subscribers_mtx);
            auto &ref_subscribers = m_eventgroup_subscribers[eventgroup_id];

            auto ptr_it = std::find_if(ref_subscribers.begin(), ref_subscribers.end(),
                                   [&ref_subscriber](const auto &ref_endpoint)
                                   {
                                       return ref_subscriber == ref_endpoint;
                                   });

            if (ptr_it == ref_subscribers.end())
            {
                if (ref_subscribers.size() >= MULTICAST_THRESHOLD)
                {
                }
                if (ref_subscribers.size() >= ref_subscribers.capacity())
                {
                    // network-driven, reject rather than trip the capacity assert
                    SD_SERVICE_LOGGER::error("eventgroup %u subscriber list full (MAX_SUBSCRIBERS_PER_EVENTGROUP); rejecting", eventgroup_id);
                    return false;
                }
                ref_subscribers.push_back(ref_subscriber);
                return true;
            }
            else
            {
                return false;
            }
        }

        Eventgroup* SdService::get_eventgroup(uint16_t eventgroup_id) {
  auto ptr_it = m_eventgroups.find(eventgroup_id);
  if (ptr_it == m_eventgroups.end())
    return nullptr;

  return &(ptr_it->second);
}

void SdService::register_event(uint16_t event_id, uint16_t eventgroup_id, bool on_change, TransportKind transport_kind){
  m_events[event_id] = Event{event_id, eventgroup_id, on_change, transport_kind};
  m_eventgroups[eventgroup_id].event_ids.push_back(event_id);
}

event_payload SdService::get_event_payload(uint16_t event_id) const {
  os::Guard lock(m_events_mtx);
  auto ptr_it = m_events.find(event_id);
  if (ptr_it == m_events.end())
    return event_payload{};

  return ptr_it->second.payload;
}

bool SdService::set_event_payload(uint16_t event_id, const uint8_t *ptr_data, size_t len){
  os::Guard lock(m_events_mtx);
  auto ptr_it = m_events.find(event_id);
  if (ptr_it == m_events.end())
    return false;

  auto& ref_event = ptr_it->second;
  if (len > ref_event.payload.capacity())
    len = ref_event.payload.capacity();

  bool changed = ref_event.payload.size() != len;
  if (!changed) {
    for (size_t i = 0; i < len; ++i)
      if (ref_event.payload[i] != ptr_data[i]) { changed = true; break; }
  }
  if (changed) {
    ref_event.payload.clear();
    for (size_t i = 0; i < len; ++i) ref_event.payload.push_back(ptr_data[i]);
  }

  return changed && ref_event.on_change;
}

const Event* SdService::get_event(uint16_t event_id) const {
  auto ptr_it = m_events.find(event_id);
  if (ptr_it == m_events.end())
    return nullptr;

  return &(ptr_it->second);
}

    } // namespace sd
} // namespace someIp
