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

// handles SD eventgroup subscriptions for client and server side
#ifndef SOMEIP_SD_SUBSCRIPTION_MANAGER_HPP_
#define SOMEIP_SD_SUBSCRIPTION_MANAGER_HPP_

#include <memory>
#include "someIp/utils/PoolPtr.hpp"

#include "Def.hpp"
#include "EventgroupEntry.hpp"
#include "SdServiceRegistry.hpp"
#include "SdTransport.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/config/StackConfig.hpp"
#include "someIp/os/Os.hpp"

namespace someIp
{
    namespace sd
    {
        class Ipv4Option;
        class Ipv6Option;

        // filled only when a new subscriber was added
        struct SubscribeOutcome
        {
            bool added = false;
            SdServiceInfo service{};
            uint16_t eventgroup_id = 0;
            SdSubscriberInfo subscriber{};
        };

        class SdSubscriptionManager
        {
        public:
            SdSubscriptionManager(SdServiceRegistry &ref_registry, SdTransport &ref_transport);
            ~SdSubscriptionManager();

            SdSubscriptionManager(const SdSubscriptionManager &) = delete;
            SdSubscriptionManager &operator=(const SdSubscriptionManager &) = delete;

            /* client side */
            void subscribe(const SdEventgroupInfo &ref_eventgroup, OptionVec options = {});
            void unsubscribe_all();
            // processes a SUBSCRIBE_EVENTGROUP_ACK/NACK entry, returns true if ACK recorded
            bool handle_subscribe_ack(someIp::PoolPtr<EventgroupEntry> sprt_entry);
            void remove_remote_subscription(const SdServiceInfo &ref_info);

            /* server side */
            SubscribeOutcome handle_subscribe_eventgroup(someIp::PoolPtr<EventgroupEntry> sprt_entry,
                                                         const SdEndpointInfo &ref_sender);
            bool send_eventgroup_ack(const SdEndpointInfo &ref_dest_endpoint,
                                     const SdEventgroupInfo &ref_eventgroup,
                                     someIp::PoolPtr<Ipv4Option> sprt_ipv4_multicast_option = nullptr,
                                     someIp::PoolPtr<Ipv6Option> sprt_ipv6_multicast_option = nullptr);
            bool validate_eventgroup_options(someIp::PoolPtr<EventgroupEntry> sprt_entry);

            bool get_stop_flag() const { return m_stop_flag; }

        private:
            SdServiceRegistry &m_ref_registry;
            SdTransport &m_ref_transport;
            StaticVector<SdEventgroupInfo, config::MAX_REMOTE_SERVICES> m_remote_subscriptions;
            mutable os::Mutex m_mtx;
            bool m_stop_flag = false;
        };

    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_SUBSCRIPTION_MANAGER_HPP_
