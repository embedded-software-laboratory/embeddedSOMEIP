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

// a service in the SOME/IP Service Discovery stack
#ifndef SOMEIP_SD_SERVICE_HPP_
#define SOMEIP_SD_SERVICE_HPP_

#include <cstdint>
#include "someIp/utils/PoolPtr.hpp"

#include "Def.hpp"
#include "SdMessage.hpp"
#include "someIp/structs/Service.hpp"
#include "someIp/utils/timer.hpp"
#include "someIp/utils/StaticMap.hpp"
#include "EventgroupEntry.hpp"
#include "ServiceEntry.hpp"


namespace someIp
{
    namespace sd
    {
        using SubscriberVec = StaticVector<SdSubscriberInfo, config::MAX_SUBSCRIBERS_PER_EVENTGROUP>;
        using EventIdVec = StaticVector<uint16_t, config::MAX_EVENTS_PER_EVENTGROUP>;

        class SdService : public Service
        {
        private:
            SdServiceInfo m_info;
            someIp::PoolPtr<TimerEvent> m_sprt_ttl_timer;
            OptionVec m_options = {};
            StaticMap<uint16_t, Event, config::MAX_EVENTS_PER_SERVICE> m_events; // event_id -> events
            StaticMap<uint16_t, Eventgroup, config::MAX_EVENTGROUPS_PER_SERVICE> m_eventgroups; // eventgroup_id -> event_ids
            StaticMap<uint16_t, SubscriberVec, config::MAX_EVENTGROUPS_PER_SERVICE> m_eventgroup_subscribers; // eventgroup_id -> subscribers
            someIp::PoolPtr<ServiceEntry> m_sprt_service_entry = nullptr;
            SdEndpointInfo m_endpoint;
            SdEndpointInfo m_sender_info;
            uint16_t m_priority = SD_LOWEST_PRIORITY;
            uint16_t m_weight = 0;
            bool m_ready_for_offering = true;
            someIp::Mutex m_subscribers_mtx;
            mutable someIp::Mutex m_events_mtx;
            // server side INITIAL_WAIT -> REPETITION -> MAIN
            SdPhase m_status = SdPhase::NOT_READY;
            uint8_t m_repetitions = 0;
            Duration m_total_delay;
            // delay for next repetition, doubles each time (exponential backoff)
            Duration m_current_delay = Duration(config::REPETITIONS_BASE_DELAY);

        public:
            SdService(const SdServiceInfo &ref_service_info);
            SdService(const SdServiceInfo &ref_service_info, const SdEndpointInfo &ref_endpoint);
            SdService(const SdServiceInfo &ref_service_info, const OptionVec &ref_options);
            SdService(const SdServiceInfo &ref_service_info, someIp::PoolPtr<TimerEvent> sprt_timer, const OptionVec &ref_options);

            void notify_all(uint16_t eventgroup_id, const void *ptr_data, size_t size);

            bool add_subscriber(uint16_t eventgroup_id, const SdSubscriberInfo &ref_subscriber);
            bool remove_subscriber(uint16_t eventgroup_id, const SdSubscriberInfo &ref_subscriber);

            void register_event(uint16_t event_id, uint16_t eventgroup_id, bool on_change = false, TransportKind transport_kind = TransportKind::DEFAULT);

            // set event payload, returns whether a notification should be sent
            bool set_event_payload(uint16_t event_id, const uint8_t *ptr_data, size_t len);
            const Event* get_event(uint16_t event_id) const;
            // fixed-capacity copy of the event payload (empty if event unknown)
            event_payload get_event_payload(uint16_t event_id) const;

            Eventgroup* get_eventgroup(uint16_t eventgroup_id);
            // empty when eventgroup has no events or subscribers yet
            EventIdVec get_events_of_eventgroup(uint16_t eventgroup_id) const {
                auto ptr_it = m_eventgroups.find(eventgroup_id);
                return ptr_it != m_eventgroups.end() ? ptr_it->second.event_ids : EventIdVec{};
            }
            SubscriberVec get_subscribers_of_eventgroup(uint16_t eventgroup_id) const {
                auto ptr_it = m_eventgroup_subscribers.find(eventgroup_id);
                return ptr_it != m_eventgroup_subscribers.end() ? ptr_it->second : SubscriberVec{};
            }

            bool find_eventgroup(uint16_t eventgroup_id) const { return m_eventgroups.find(eventgroup_id) != m_eventgroups.end(); }
 

            const OptionVec &get_options() const { return m_options; }
            void update_options(OptionVec new_options) { m_options = new_options; }

            uint16_t get_service_id() const { return m_info._service_id; }

            uint16_t get_instance_id() const { return m_info._instance_id; }

            // full 32-bit TTL, uint16 truncation broke SD_MAX_TTL compares (M-5)
            uint32_t get_ttl() const { return m_info._ttl; }
            void set_ttl(uint32_t ttl) { m_info._ttl = ttl; }

            uint8_t get_major_version() const { return m_info._major_version; }

            uint32_t get_minor_version() const { return m_info._minor_version; }

            SdServiceInfo get_info() const { return m_info; }

            uint16_t get_port() const { return m_info._src_port; }

            SdPhase get_status() const { return m_status; }
            void set_status(SdPhase status) { m_status = status; }

            someIp::PoolPtr<TimerEvent> get_timer() const { return m_sprt_ttl_timer; }
            void set_timer(someIp::PoolPtr<TimerEvent> sprt_timer) { m_sprt_ttl_timer = sprt_timer; }

            someIp::PoolPtr<ServiceEntry> get_service_entry() const { return m_sprt_service_entry; }
            void set_service_entry(someIp::PoolPtr<ServiceEntry> sprt_ServiceEntry) { m_sprt_service_entry = sprt_ServiceEntry; }

            SdEndpointInfo get_endpoint() const { return m_endpoint; }

            SdEndpointInfo get_sender_info() const { return m_sender_info; }
            void set_sender_info(const SdEndpointInfo &ref_sender_info) { m_sender_info = ref_sender_info; }

            uint16_t get_priority() const { return m_priority; }
            void set_priority(uint16_t priority) { m_priority = priority; }

            uint16_t get_weight() const { return m_weight; }
            void set_weight(uint16_t weight) { m_weight = weight; }

            void set_ready(bool ready) { m_ready_for_offering = ready; }

            std::string _get_status_name() const;

            uint8_t get_repetition_count() const { return m_repetitions; }

            Duration get_total_delay() const { return m_total_delay; }

            Duration get_current_delay() const { return m_current_delay; }

            void add_to_total_delay(Duration delay) { m_total_delay += delay; }
            void increment_repetition() { m_repetitions++; }
            void double_delay() { m_current_delay *= 2; }
        };
    } // sd
} // namespace someIp

#endif // SOMEIP_SD_SERVICE_HPP_
