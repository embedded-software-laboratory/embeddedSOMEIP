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

#include "SdPool.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include "SdSubscriptionManager.hpp"

#include <algorithm>

#include "IpOption.hpp"
#include "Ipv4Option.hpp"
#include "Ipv6Option.hpp"
#include "someIp/config/config.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/os/Os.hpp"

#include "someip_tp.h"

constexpr char SD_SUBSCRIPTION_MANAGER_TAG[] = "SD_SUBSCRIPTION_MANAGER";
using SD_SUBSCRIPTION_LOGGER = BaseLogger<SD_SUBSCRIPTION_MANAGER_TAG>;

namespace someIp
{
    namespace sd
    {

        SdSubscriptionManager::SdSubscriptionManager(SdServiceRegistry &ref_registry, SdTransport &ref_transport)
            : m_ref_registry(ref_registry), m_ref_transport(ref_transport)
        {
        }

        SdSubscriptionManager::~SdSubscriptionManager()
        {
        }

        void SdSubscriptionManager::subscribe(const SdEventgroupInfo &ref_eventgroup, OptionVec options)
        {
            SD_SUBSCRIPTION_LOGGER::debug_sd(
                "number of remote services: %zu",
                m_ref_registry.remote_count());
            auto sprt_service_to_subscribe = m_ref_registry.find_remote(SdServiceInfo{ref_eventgroup._service_id, ref_eventgroup._instance_id, ref_eventgroup._major_version, SD_ANY_MINOR_VERSION});
            if (sprt_service_to_subscribe)
            {
                SD_SUBSCRIPTION_LOGGER::debug_sd(
                    "subscribing to service: %u",
                    ref_eventgroup._service_id);
            }
            else
            {
                SD_SUBSCRIPTION_LOGGER::error("No service available with this info: ID: %d, Instance: %d, major: %d, eventgroupID: %d",
                                              ref_eventgroup._service_id, ref_eventgroup._instance_id, ref_eventgroup._major_version, ref_eventgroup._eventgroup_id);
                return;
            }

            SdMessage msg;
            auto sprt_entry = pool_make<EventgroupEntry>(SdEntryType::SUBSCRIBE_EVENTGROUP, ref_eventgroup);
            if (!options.empty())
            {
                sprt_entry->update_options(options);
                validate_eventgroup_options(sprt_entry);
            }
            msg.add_entry(sprt_entry);
            if (!sprt_entry->get_options().empty())
            {
                msg.add_entry_options(sprt_entry, SD_FIRST_OPTION_RUN, sprt_entry->get_options());
            }
            lttng_ust_tracepoint(someip, eventgroup_subscribe_sent,
                                 ref_eventgroup._service_id,
                                 ref_eventgroup._instance_id,
                                 ref_eventgroup._major_version,
                                 ref_eventgroup._eventgroup_id,
                                 ref_eventgroup._ttl);
            // SubscribeEventgroup is sent unicast to the provider's SD endpoint
            const IpAddr provider_ip = sprt_service_to_subscribe->get_sender_info()._ip;
            // all SD traffic must originate from SD_PORT
            m_ref_transport.send_package(msg, provider_ip, config::SD_PORT, config::SD_PORT);
        }

        void SdSubscriptionManager::unsubscribe_all()
        {
            SdMessage msg;
            os::Guard lock(m_mtx);
            if (m_remote_subscriptions.empty())
                return;
            for (auto &ref_subscription : m_remote_subscriptions)
            {
                ref_subscription._ttl = SD_END_TTL;
                auto sprt_entry = pool_make<EventgroupEntry>(SdEntryType::STOP_SUBSCRIBE_EVENTGROUP, ref_subscription);
                msg.add_entry(sprt_entry);
            }
            IpAddr multicast_ip;
            IpAddr::from_string(config::SD_MULTICAST_IP, multicast_ip);
            m_ref_transport.send_package(msg, multicast_ip, config::SD_PORT, config::SD_PORT);
        }

        bool SdSubscriptionManager::handle_subscribe_ack(someIp::PoolPtr<EventgroupEntry> sprt_entry)
        {
            SD_SUBSCRIPTION_LOGGER::debug_sd(
                "############################################################################");
            lttng_ust_tracepoint(someip, subscribe_ack_received,
                                 sprt_entry->get_eventgroup_info()._service_id,
                                 sprt_entry->get_eventgroup_info()._instance_id,
                                 sprt_entry->get_eventgroup_info()._major_version,
                                 sprt_entry->get_eventgroup_info()._eventgroup_id,
                                 sprt_entry->get_ttl());
            const SdEventgroupInfo info = sprt_entry->get_eventgroup_info();
            auto matches = [&](const SdEventgroupInfo &ref_eg)
            {
                return ref_eg._service_id == info._service_id &&
                       ref_eg._instance_id == info._instance_id &&
                       ref_eg._major_version == info._major_version &&
                       ref_eg._eventgroup_id == info._eventgroup_id;
            };
            if (sprt_entry->get_ttl())
            {
                SD_SUBSCRIPTION_LOGGER::debug_sd("RECEIVED ACK_SUBSCRIBE_EVENTGROUP message");
                SD_SUBSCRIPTION_LOGGER::debug_sd("############################################################################");

                os::Guard lock(m_mtx);
                // duplicate ACK must not grow the vector, it asserts when full
                if (std::none_of(m_remote_subscriptions.begin(), m_remote_subscriptions.end(), matches))
                    m_remote_subscriptions.push_back(info);
                return true;
            }
            SD_SUBSCRIPTION_LOGGER::debug_sd("RECEIVED NACK_SUBSCRIBE_EVENTGROUP message");
            SD_SUBSCRIPTION_LOGGER::debug_sd("############################################################################");
            {
                // subscription ended, drop our record of it
                os::Guard lock(m_mtx);
                m_remote_subscriptions.erase(
                    std::remove_if(m_remote_subscriptions.begin(), m_remote_subscriptions.end(), matches),
                    m_remote_subscriptions.end());
            }
            return false;
        }

        void SdSubscriptionManager::remove_remote_subscription(const SdServiceInfo &ref_info)
        {
            os::Guard lock(m_mtx);
            m_remote_subscriptions.erase(
                std::remove_if(m_remote_subscriptions.begin(), m_remote_subscriptions.end(),
                               [&](const SdEventgroupInfo &ref_eg)
                               {
                                   return ref_eg._service_id == ref_info._service_id &&
                                          (ref_eg._instance_id == ref_info._instance_id || ref_info._instance_id == SD_ANY_INSTANCE_ID) &&
                                          (ref_eg._major_version == ref_info._major_version || ref_info._major_version == SD_ANY_MAJOR_VERSION);
                               }),
                m_remote_subscriptions.end());
        }

        SubscribeOutcome SdSubscriptionManager::handle_subscribe_eventgroup(someIp::PoolPtr<EventgroupEntry> sprt_entry,
                                                                            const SdEndpointInfo &ref_sender)
        {
            someIp::PoolPtr<SdService> sprt_service_to_subscribe;
            SdEventgroupInfo eventgroup = sprt_entry->get_eventgroup_info();
            SD_SUBSCRIPTION_LOGGER::debug_sd(
                "RECEIVED SUBSCRIBE EVENTGROUP FOR EVENTGROUP_ID: %u",
                sprt_entry->get_eventgroup_id());
            if (sprt_entry->get_options().empty() || !validate_eventgroup_options(sprt_entry))
            {
                SD_SUBSCRIPTION_LOGGER::error("RECEIVED SUBSCRIBE_EVENTGROUP_ENTRY WITH INVALID IP OPTION");
                // send NACK
                eventgroup._ttl = SD_END_TTL;
                return {};
            }
            someIp::PoolPtr<IpOption> sprt_endpoint_option = nullptr;
            someIp::PoolPtr<IpOption> sprt_tcp_endpoint_option = nullptr;
            for (const auto &ref_opt : sprt_entry->get_options())
            {
                auto sprt_endpoint = (ref_opt && is_ip_option(ref_opt->get_type())) ? someIp::pool_static_pointer_cast<IpOption>(ref_opt) : someIp::PoolPtr<IpOption>();
                if (!sprt_endpoint)
                    continue;
                if (!sprt_endpoint_option)
                    sprt_endpoint_option = sprt_endpoint;
                if (sprt_endpoint->is_tcp())
                {
                    sprt_tcp_endpoint_option = sprt_endpoint;
                    break;
                }
            }
            if (!sprt_endpoint_option)
            {
                SD_SUBSCRIPTION_LOGGER::error("RECEIVED SUBSCRIBE_EVENTGROUP_ENTRY WITHOUT VALID IP OPTION");
                eventgroup._ttl = SD_END_TTL;
                return {};
            }
            auto sprt_subscriber_endpoint_option = sprt_tcp_endpoint_option ? sprt_tcp_endpoint_option : sprt_endpoint_option;
            // ACK goes to the subscriber's SD port, not the event endpoint
            const SdEndpointInfo ack_endpoint(ref_sender._ip, config::SD_PORT);

            sprt_service_to_subscribe = m_ref_registry.find_separate_by_eventgroup(sprt_entry->get_eventgroup_info());
            if (sprt_service_to_subscribe)
            {
                SD_SUBSCRIPTION_LOGGER::debug_sd(
                    "----------------------------------------------------------------\n\n"
                    "FOUND SERVICE WITH EVENTGROUP INFO\n"
                    "----------------------------------------------------------------");
            }
            else
            {
                SD_SUBSCRIPTION_LOGGER::debug_sd(
                    "SERVICE WITH EVENTGROUP INFO WASN'T FOUND IN SEPARATE SERVICES");
                SD_SUBSCRIPTION_LOGGER::debug_sd(
                    "NUMBER OF OFFERED_SERVICES: %zu",
                    m_ref_registry.offered_count());

                sprt_service_to_subscribe = m_ref_registry.find_offered_by_eventgroup(sprt_entry->get_eventgroup_info());
                if (!sprt_service_to_subscribe)
                {
                    eventgroup._ttl = SD_END_TTL;
                    SD_SUBSCRIPTION_LOGGER::debug_sd("SERVICE WASN'T FOUND");
                    send_eventgroup_ack(ack_endpoint, eventgroup);
                    return {};
                }
                SD_SUBSCRIPTION_LOGGER::debug_sd(
                    "----------------------------------------------------------------\n\n"
                    "FOUND SERVICE WITH EVENTGROUP INFO\n"
                    "----------------------------------------------------------------");
            }
            // if (service_to_subscribe->get_status() != SdPhase::MAIN)
            // {
            //     SD_SUBSCRIPTION_LOGGER::error("Service is not in MAIN phase, going to be ignored");
            //     eventgroup._ttl = SD_END_TTL;
            //     send_eventgroup_ack(endpoint_option->get_endpoint(), eventgroup);
            //     return;
            // }
            auto subscriber_info = SdSubscriberInfo(ref_sender, sprt_subscriber_endpoint_option->get_endpoint());
            if (sprt_entry->get_ttl() == SD_END_TTL)
            {
                // STOP_SUBSCRIBE_EVENTGROUP
                if (sprt_service_to_subscribe->remove_subscriber(sprt_entry->get_eventgroup_id(), subscriber_info))
                {
                    IpAddr ip = sprt_subscriber_endpoint_option->get_ip_address();
                    SD_SUBSCRIPTION_LOGGER::debug_sd(
                        "SUCCESSFULLY DELETED SUBSCRIBER WITH IP: %s, PORT: %u",
                        ip.c_str(),
                        sprt_subscriber_endpoint_option->get_port());
                }
                else
                {
                    SD_SUBSCRIPTION_LOGGER::error("RECEIVED SUBSCRIBE_EVENTGROUP WITH TTL 0, OR THE SUBSCRIBER WAS NOT FOUND");
                    // send NACK
                    eventgroup._ttl = SD_END_TTL;
                    send_eventgroup_ack(ack_endpoint, eventgroup);
                }
            }
            else
            {
                // SUBSCRIBE_EVENTGROUP
                bool successful = sprt_service_to_subscribe->add_subscriber(sprt_entry->get_eventgroup_id(), subscriber_info);
                if (successful)
                {
                    send_eventgroup_ack(ack_endpoint, eventgroup);
                    IpAddr ip = sprt_subscriber_endpoint_option->get_ip_address();
                    SD_SUBSCRIPTION_LOGGER::debug_sd(
                        "SUCCESSFULLY ADD SUBSCRIBER WITH ENDPOINT WITH IP: %s, PORT: %u, SUBSCRIBED TO EVENTGROUP: %u",
                        ip.c_str(),
                        sprt_subscriber_endpoint_option->get_port(),
                        sprt_entry->get_eventgroup_id());
                    // ACK first, then the caller may push the initial field values
                    return SubscribeOutcome{true,
                                            sprt_service_to_subscribe->get_info(),
                                            sprt_entry->get_eventgroup_id(),
                                            subscriber_info};
                }
                else
                {
                    // send NACK
                    eventgroup._ttl = SD_END_TTL;
                    send_eventgroup_ack(ack_endpoint, eventgroup);
                }
            }
            return {};
        }

        bool SdSubscriptionManager::send_eventgroup_ack(const SdEndpointInfo &ref_dest_endpoint,
                                                          const SdEventgroupInfo &ref_eventgroup,
                                                          someIp::PoolPtr<Ipv4Option> sprt_ipv4_multicast_option,
                                                          someIp::PoolPtr<Ipv6Option> sprt_ipv6_multicast_option)
        {
            lttng_ust_tracepoint(someip, subscribe_ack_sent,
                                 ref_eventgroup._service_id,
                                 ref_eventgroup._instance_id,
                                 ref_eventgroup._major_version,
                                 ref_eventgroup._eventgroup_id,
                                 ref_eventgroup._ttl);
            SdMessage msg;
            auto sprt_entry = pool_make<EventgroupEntry>(SdEntryType::SUBSCRIBE_EVENTGROUP_ACK, ref_eventgroup);
            msg.add_entry(sprt_entry);
            if (ref_eventgroup._ttl == SD_END_TTL)
            {
                SD_SUBSCRIPTION_LOGGER::debug_sd(
                    "SENDING NACK TO: %s:%u",
                    ref_dest_endpoint._ip.c_str(),
                    ref_dest_endpoint._port);
                m_stop_flag = true;
                return m_ref_transport.send_package(msg, ref_dest_endpoint._ip, ref_dest_endpoint._port, config::LOCAL_PORT);
            }
            OptionVec valid_options;
            if (ref_eventgroup.has_multicast_events)
            {
                if (sprt_ipv4_multicast_option && sprt_ipv4_multicast_option->get_type() == SdOptionType::IPV4_MULTICAST)
                {
                    valid_options.push_back(sprt_ipv4_multicast_option);
                }
                if (sprt_ipv6_multicast_option && sprt_ipv6_multicast_option->get_type() == SdOptionType::IPV6_MULTICAST)
                {
                    valid_options.push_back(sprt_ipv6_multicast_option); // was ipv4 (copy-paste, report M-8)
                }
                if (valid_options.empty())
                {
                    SD_SUBSCRIPTION_LOGGER::error("sending eventgroup acknowledge for eventgroup containing multicast events without any multicast options, this should not happen");
                }
            }
            if (!valid_options.empty())
            {
                msg.add_entry_options(sprt_entry, SD_FIRST_OPTION_RUN, valid_options);
            }
            SD_SUBSCRIPTION_LOGGER::debug_sd(
                "SENDING ACK TO: %s:%u",
                ref_dest_endpoint._ip.c_str(),
                ref_dest_endpoint._port);
            m_stop_flag = true;
            return m_ref_transport.send_package(msg, ref_dest_endpoint._ip, ref_dest_endpoint._port, config::SD_PORT);
        }

        bool SdSubscriptionManager::validate_eventgroup_options(someIp::PoolPtr<EventgroupEntry> sprt_entry)
        {
            OptionVec valid_options;

            int endpoint_count = 0;      // 0-2 allowed
            int multicast_count = 0;     // 0-1 allowed
            int configuration_count = 0; // 0-1 allowed

            const auto &ref_options = sprt_entry->get_options();

            auto is_valid_endpoint = [](const someIp::PoolPtr<Option> &ref_opt) -> bool
            {
                auto sprt_endpoint = (ref_opt && is_ip_option(ref_opt->get_type())) ? someIp::pool_static_pointer_cast<IpOption>(ref_opt) : someIp::PoolPtr<IpOption>();
                if (!sprt_endpoint)
                    return false;
                if (sprt_endpoint->is_tcp() && !config::TCP_ENABLED)
                    return false;
                return true;
            };

            for (const auto &ref_opt : ref_options)
            {
                switch (ref_opt->get_type())
                {
                case SdOptionType::IPV4_ENDPOINT:
                    if (config::IPV4_ENABLED && endpoint_count < 2 && is_valid_endpoint(ref_opt))
                    {
                        valid_options.push_back(ref_opt);
                        endpoint_count++;
                    }
                    break;

                case SdOptionType::IPV6_ENDPOINT:
                    if (config::IPV6_ENABLED && endpoint_count < 2 && is_valid_endpoint(ref_opt))
                    {
                        valid_options.push_back(ref_opt);
                        endpoint_count++;
                    }
                    break;

                case SdOptionType::IPV4_MULTICAST:
                    if (config::IPV4_ENABLED && multicast_count < 1)
                    {
                        valid_options.push_back(ref_opt);
                        multicast_count++;
                    }
                    break;

                case SdOptionType::IPV6_MULTICAST:
                    if (config::IPV6_ENABLED && multicast_count < 1)
                    {
                        valid_options.push_back(ref_opt);
                        multicast_count++;
                    }
                    break;

                case SdOptionType::CONFIGURATION:
                    // require at least one valid ip option
                    if (configuration_count < 1 && !valid_options.empty())
                    {
                        valid_options.push_back(ref_opt);
                        configuration_count++;
                    }
                    break;

                case SdOptionType::LOAD_BALANCING:
                    SD_SUBSCRIPTION_LOGGER::error("Eventgroup can't have load balancing options, going to be ignored");
                    break;

                default:
                    SD_SUBSCRIPTION_LOGGER::error("Eventgroup has unsupported option type: ");
                    ref_opt->print_type();
                    SD_SUBSCRIPTION_LOGGER::error(", going to be ignored");
                }
            }
            sprt_entry->update_options(valid_options);
            if (valid_options.empty() && !ref_options.empty())
                return false;
            bool has_udp_endpoint = false;

            for (const auto &ref_opt : valid_options)
            {
                if ((ref_opt->get_type() == SdOptionType::IPV4_ENDPOINT || ref_opt->get_type() == SdOptionType::IPV6_ENDPOINT))
                {
                    auto sprt_endpoint = (ref_opt && is_ip_option(ref_opt->get_type())) ? someIp::pool_static_pointer_cast<IpOption>(ref_opt) : someIp::PoolPtr<IpOption>();
                    if (sprt_endpoint && !sprt_endpoint->is_tcp())
                    {
                        has_udp_endpoint = true;
                        break;
                    }
                }
            }

            if (!has_udp_endpoint && config::SD_MULTICAST_THRESHOLD != 1)
            {
                SD_SUBSCRIPTION_LOGGER::error("Rejected SUBSCRIBE_EVENTGROUP without UDP endpoint and MULTICAST_THRESHOLD != 1");
                return false;
            }
            return true;
        }

    } // namespace sd
} // namespace someIp
