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

#include "SdManager.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include <cstdio>
#include "SdPool.hpp"
#include "SdMessage.hpp"

#include "ServiceEntry.hpp"
#include "EventgroupEntry.hpp"

#include "ConfigurationOption.hpp"
#include "LoadBalancingOption.hpp"
#include "Ipv6Option.hpp"
#include "Ipv4Option.hpp"

#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/config/config.hpp"
#include <cstdint>
#include <memory>
#include <sstream>

constexpr char CONFIG_OPTION_TAG[] = "SERVICE_DISCOVERY_MANAGER";
using SD_MANAGER_LOGGER = BaseLogger<CONFIG_OPTION_TAG>;

namespace someIp
{
    namespace sd
    {

        SdManager::SdManager(ThreadPool *ptr_thread_pool, IpAddr local_ip, const ServiceVec &ref_services) : m_registry(ref_services), m_subscriptions(m_registry, *this), m_find(*this, m_event_manager), m_offer(m_registry, *this, m_event_manager), m_ptr_thread_pool(ptr_thread_pool), m_local_ip(local_ip)
        {
            m_link_status = SdInterfaceStatus::LINK_UP;
            m_event_manager.start();
            IpAddr::from_string(config::SD_MULTICAST_IP, m_multicast_ip);
        }


        bool SdManager::_offer_service(someIp::PoolPtr<SdService> sprt_service,
                                        const IpAddr &ref_dest_ip,
                                        uint16_t dest_port)
        {
            if (m_link_status != SdInterfaceStatus::LINK_UP)
            {
                SD_MANAGER_LOGGER::error("interface is not ready");
                return false;
            }
            // stored entry is shared, option indices are per message
            SdMessage msg;
            auto sprt_entry = pool_make<ServiceEntry>(SdEntryType::OFFER_SERVICE, sprt_service->get_info());
            msg.add_entry(sprt_entry);
            msg.overwrite_entry_options(sprt_entry, SD_FIRST_OPTION_RUN, sprt_service->get_options());

            return send_package(msg, ref_dest_ip, dest_port);
        }

        bool SdManager::send_find_service(const SdServiceInfo &ref_service_info,
                                           someIp::PoolPtr<ConfigurationOption> sprt_config_option)
        {
            return m_find.request(ref_service_info, sprt_config_option);
        }

        bool SdManager::offer_service(someIp::PoolPtr<SdService> sprt_service,
                                       const IpAddr &ref_src_ip,
                                       uint16_t src_port,
                                       OptionVec options_)
        {
            if (m_link_status != SdInterfaceStatus::LINK_UP)
            {
                SD_MANAGER_LOGGER::error("interface is not ready");
                return false;
            }
            return m_offer.start_offer(sprt_service, ref_src_ip, src_port, options_);
        }


        void SdManager::remove_remote_subscription(const SdServiceInfo &ref_info)
        {
            m_subscriptions.remove_remote_subscription(ref_info);
        }

        void SdManager::stop_all_services()
        {
            m_offer.stop_all();
        }

        bool SdManager::stop_offer_service(SdServiceInfo service_info)
        {
            return m_offer.stop_offer(service_info);
        }

        void SdManager::register_client_service(someIp::PoolPtr<SdClientService> sprt_client)
        {
            m_find.register_client(sprt_client);
        }

        void SdManager::register_service(someIp::PoolPtr<SdService> sprt_service)
        {
            m_registry.register_service(sprt_service);
        }

        void SdManager::unregister_service(someIp::PoolPtr<SdService> sprt_service)
        {
            m_registry.unregister_service(sprt_service);
        }

        bool SdManager::validate_offer_service(const SdServiceInfo &ref_service_info)
        {
            return m_registry.validate_offer_service(ref_service_info);
        }

        bool SdManager::find_local_port(uint16_t port)
        {
            return m_registry.find_local_port(port);
        }

        bool SdManager::validate_eventgroup_options(someIp::PoolPtr<EventgroupEntry> sprt_entry)
        {
            return m_subscriptions.validate_eventgroup_options(sprt_entry);
        }

        bool SdManager::offer_local_services()
        {
            return m_offer.offer_local_services(m_local_ip);
        }

        void SdManager::validate_offer_service_options(OptionVec &ref_valid_options, const OptionVec &ref_options)
        {
            SdOfferManager::validate_offer_service_options(ref_valid_options, ref_options);
        }

        bool SdManager::send_package(SdMessage msg, IpAddr dest_ip, uint16_t dest_port, uint16_t src_port)
        {
            lttng_ust_tracepoint(someip, package_sent, msg.get_length(), dest_ip.c_str(), dest_port);
            if (config::DEBUG_SD_LOG_ENABLED)
                printf("\n\n");
            SD_MANAGER_LOGGER::debug_sd("----------------------------------------------------------------------------");
            SD_MANAGER_LOGGER::debug_sd("<<<<<<<<<<<< SENDING MESSAGE of size: %u >>>>>>>>>>>", msg.get_length());
            SD_MANAGER_LOGGER::debug_sd("----------------------------------------------------------------------------");

            SdEndpointInfo endpoint(dest_ip, dest_port);
            bool reboot_flag = false;
            // per-destination incrementing session id, needed for reboot detection
            uint16_t sid = m_sessions.next_sender_sid(endpoint, &reboot_flag);
            msg.set_session_id(sid);
            reboot_flag ? msg.set_reboot_flag() : msg.reset_reboot_flag();

            Package p(msg, dest_ip, dest_port);
            auto buf = p.serialize();
            auto tx = PackageTx(std::move(buf), p.getDestinationIp(), p.getDestinationPort(), src_port);
            // auto endpoint = std::make_shared<SdEndpointInfo>(p.getDestinationIp(), p.getDestinationPort());
            if (!m_ptr_thread_pool->send_package(std::move(tx))){
                SD_MANAGER_LOGGER::error("Failed to send package");
                return false;
            } else {
                SD_MANAGER_LOGGER::debug_sd("Message sent successfully to [%s:%u]",
                                            dest_ip.c_str(),
                                            dest_port);
            }
           return true;
        }

        uint16_t SdManager::update_senders_sid(const SdEndpointInfo &ref_endpoint)
        {
            return m_sessions.next_sender_sid(ref_endpoint);
        }

        void SdManager::handle_find_service(const ServiceEntry &ref_entry)
        {
            SdServiceInfo service_info = ref_entry.get_service_info();
            SD_MANAGER_LOGGER::debug("received FIND_SERVICE message for Service ID: %d, Instance ID: %d, Major Version: %d",
                                     service_info._service_id,
                                     service_info._instance_id,
                                     service_info._major_version);
            SD_MANAGER_LOGGER::debug_sd(
                "received FIND_SERVICE message for Service ID: %u, Instance ID: %u, Major Version: %d, Minor Version: %d",
                service_info._service_id,
                service_info._instance_id,
                static_cast<int>(service_info._major_version),
                service_info._minor_version);

            auto sprt_service = m_registry.find_local_service(service_info);
            // answer only if the service's own offer machine reached the Main Phase
            if (sprt_service && sprt_service->get_status() == SdPhase::MAIN)
            {
                SD_MANAGER_LOGGER::debug_sd("Local service found, sending OFFER_SERVICE message");
                // if ((*it)->get_status() == sd_status::MAIN || (*it)->get_status() == sd_status::REPETITION)
                // {
                //     ip_addr_t ipaddr;
                //     ipaddr_aton(config::SD_MULTICAST_IP, &ipaddr);
                //     offer_service((*it)->get_service_info(), ipaddr, config::SD_PORT);
                // }
                IpAddr ipaddr;
                IpAddr::from_string(config::SD_MULTICAST_IP, ipaddr);
                // not offer_service(), its ip/port are the advertised source endpoint
                _offer_service(sprt_service, ipaddr, config::SD_PORT);
            }

            else
            {
                SD_MANAGER_LOGGER::debug_sd("Local service not found, no action taken");
            }
        }

        void SdManager::handle_subscribe_eventgroup(someIp::PoolPtr<EventgroupEntry> sprt_entry,
                                                     const SdEndpointInfo &ref_sender)
        {
            SubscribeOutcome outcome = m_subscriptions.handle_subscribe_eventgroup(sprt_entry, ref_sender);
            if (config::SD_INITIAL_NOTIFY_ON_SUBSCRIBE && outcome.added && m_on_new_subscriber)
            {
                m_on_new_subscriber(outcome.service, outcome.eventgroup_id, outcome.subscriber);
            }
        }

        bool SdManager::send_eventgroup_ack(const SdEndpointInfo &ref_dest_endpoint,
                                             const SdEventgroupInfo &ref_eventgroup,
                                             someIp::PoolPtr<Ipv4Option> sprt_ipv4_multicast_option,
                                             someIp::PoolPtr<Ipv6Option> sprt_ipv6_multicast_option)
        {
            return m_subscriptions.send_eventgroup_ack(ref_dest_endpoint, ref_eventgroup, sprt_ipv4_multicast_option, sprt_ipv6_multicast_option);
        }

        void SdManager::unsubscribe_all()
        {
            m_subscriptions.unsubscribe_all();
        }

        void SdManager::subscribe(const SdEventgroupInfo &ref_eventgroup, OptionVec options)
        {
            m_subscriptions.subscribe(ref_eventgroup, options);
        }

        void SdManager::on_ttl_expire(SdServiceInfo service_info, const SdEndpointInfo &ref_endpoint, const SdEndpointInfo &ref_sender_endpoint)
        {
            auto sprt_service_to_remove = m_registry.remove_remote(service_info, ref_endpoint, ref_sender_endpoint);
            if (sprt_service_to_remove)
            {
                SD_MANAGER_LOGGER::debug_sd(
                    "[ON_TTL_EXPIRE]: REMOVING SERVICE: %u",
                    service_info._service_id);
            }
            m_find.on_remote_lost(service_info);
        }

        void SdManager::handle_offer_service(const SdServiceInfo &ref_service, const SdEndpointInfo &ref_sender_endpoint, const OptionVec &ref_options)
        {
            if (ref_options.empty())
            {
                SD_MANAGER_LOGGER::error("Received OFFER_SERVICE message with no options");
                return;
            }

            if (ref_options[0]->get_type() != SdOptionType::IPV4_ENDPOINT &&
                ref_options[0]->get_type() != SdOptionType::IPV6_ENDPOINT)
            {
                SD_MANAGER_LOGGER::error("First option must be an endpoint option");
                return;
            }
            someIp::PoolPtr<IpOption> sprt_valid_endpoint = nullptr;

            for (size_t i = 0; i < ref_options.size() && i < 2; ++i)
            {
                const auto &ref_opt = ref_options[i];

                someIp::PoolPtr<IpOption> sprt_ep;
                if (ref_opt->get_type() == SdOptionType::IPV4_ENDPOINT)
                {
                    if (!config::IPV4_ENABLED)
                        continue;
                    sprt_ep = (ref_opt && is_ipv4_option(ref_opt->get_type())) ? someIp::pool_static_pointer_cast<Ipv4Option>(ref_opt) : someIp::PoolPtr<Ipv4Option>();
                }
                else if (ref_opt->get_type() == SdOptionType::IPV6_ENDPOINT)
                {
                    if (!config::IPV6_ENABLED)
                        continue;
                    sprt_ep = (ref_opt && is_ipv6_option(ref_opt->get_type())) ? someIp::pool_static_pointer_cast<Ipv6Option>(ref_opt) : someIp::PoolPtr<Ipv6Option>();
                }

                if (sprt_ep && (sprt_ep->is_tcp() ? config::TCP_ENABLED : true))
                {
                    sprt_valid_endpoint = sprt_ep;
                    break;
                }
            }

            if (!sprt_valid_endpoint)
            {
                SD_MANAGER_LOGGER::error("No valid endpoint found in first two options offer service is ignored");
                return;
            }

            SdEndpointInfo endpoint = sprt_valid_endpoint->get_endpoint();

            auto sprt_remote_info = pool_make_service<SdService>(ref_service, endpoint);
            sprt_remote_info->set_sender_info(ref_sender_endpoint);

            bool should_create_timer = false;
            someIp::PoolPtr<SdService> sprt_service_timer_to_reset = nullptr;

            // STOP_OFFER_SERVICE has TTL = 0
            if (ref_service._ttl == SD_END_TTL)
            {
                SD_MANAGER_LOGGER::debug_sd(
                    "######################## Received STOP_OFFER_SERVICE message ########################");

                auto sprt_service_to_remove = m_registry.remove_remote(ref_service, endpoint, ref_sender_endpoint);
                if (!sprt_service_to_remove)
                {
                    SD_MANAGER_LOGGER::debug_sd(
                        "Service not found in remote services, can't remove");

                    return;
                }

                if (sprt_service_to_remove && sprt_service_to_remove->get_timer())
                {
                    sprt_service_to_remove->get_timer()->disable();
                }

                remove_remote_subscription(ref_service);
                SD_MANAGER_LOGGER::debug_sd(
                    "<<< REMOVED SERVICE >>>");
                m_find.on_remote_lost(sprt_service_to_remove->get_info());
                return;
            }

            sprt_service_timer_to_reset = m_registry.refresh_remote(ref_service, endpoint, ref_options);
            if (sprt_service_timer_to_reset)
            {
                if (sprt_service_timer_to_reset->get_timer())
                    sprt_service_timer_to_reset->get_timer()->reset(ref_service._ttl);
            }

            sprt_remote_info->update_options(ref_options);
            for (const auto &ref_option : ref_options)
            {
                if (ref_option->get_type() == SdOptionType::LOAD_BALANCING)
                {
                    SD_MANAGER_LOGGER::debug_sd(
                        "Adding load balancing option to service: %u",
                        ref_service._service_id);
                    auto sprt_load_option = (ref_option && is_load_balancing_option(ref_option->get_type())) ? someIp::pool_static_pointer_cast<LoadBalancingOption>(ref_option) : someIp::PoolPtr<LoadBalancingOption>();
                    sprt_remote_info->set_priority(sprt_load_option->get_priority());
                    sprt_remote_info->set_weight(sprt_load_option->get_weight());
                }
                else if (ref_option->get_type() == SdOptionType::CONFIGURATION)
                {
                    // configuration options are parsed but not retained
                    SD_MANAGER_LOGGER::debug_sd(
                        "Ignoring configuration option on service: %u",
                        ref_service._service_id);
                }
            }
            if (sprt_service_timer_to_reset)
            {
                return;
            }
            SD_MANAGER_LOGGER::debug_sd(
                "###############################################################\n"
                "Received OFFER_SERVICE message for service: %u, %u, %d, %u\n"
                "TTL value is: %u\n"
                "###############################################################",
                ref_service._service_id,
                ref_service._instance_id,
                static_cast<int>(ref_service._major_version),
                ref_service._minor_version,
                ref_service._ttl);
            should_create_timer = m_registry.add_remote(sprt_remote_info, ref_sender_endpoint);
            if (should_create_timer)
            {
                // pooled node + inline callback, no heap
                auto sprt_timer = m_event_manager.add_timer(
                    "ttl", ref_service._ttl,
                    TimerCallback([this, info = sprt_remote_info->get_info(), endpoint = sprt_remote_info->get_endpoint(), ref_sender_endpoint]()
                                  {
                                      this->on_ttl_expire(info, endpoint, ref_sender_endpoint);
                                  }));
                sprt_remote_info->set_timer(sprt_timer);
            }
            // notify any matching client find request
            m_find.on_offer_received(sprt_remote_info->get_info());
        }

        uint16_t SdManager::update_receivers_sid(const SdEndpointInfo &ref_endpoint)
        {
            return m_sessions.update_receivers_sid(ref_endpoint);
        }

        bool SdManager::handle_endpoint_reboot(const SdEndpointInfo &ref_endpoint, bool new_reboot_flag)
        {
            bool reboot_detected = m_sessions.check_reboot(ref_endpoint, new_reboot_flag);

            if (reboot_detected)
            {
                SD_MANAGER_LOGGER::debug_sd("############################################################################");
                SD_MANAGER_LOGGER::debug_sd("------------------------------- REBOOT DETECTED ----------------------------");
                SD_MANAGER_LOGGER::debug_sd("############################################################################");
                auto services_to_remove = m_registry.purge_endpoint(ref_endpoint);
                if (services_to_remove.empty())
                {
                    SD_MANAGER_LOGGER::error("SERVICE IS NOT FOUND FOR THIS ENDPOINT");
                    return true;
                }
                for (const auto &ref_service : services_to_remove)
                {
                    if (ref_service->get_timer())
                    {
                        ref_service->get_timer()->disable();
                        // _event_manager.force_remove_timer(service->get_timer());
                    }
                }

                SD_MANAGER_LOGGER::debug_sd(
                    "<--------- REMOVED ENDPOINT AND IT'S RELATED REMOTE SERVICES --------->");
                return true;
            }
            return false;
        }

        void SdManager::recv_callback(SdManager *ptr_handler, SdMessage msg, IpAddr src_ip, uint16_t src_port)
        {
          SD_MANAGER_LOGGER::debug_sd(
              "-------------------- RECEIVED SD PACKAGE FROM [%s:%u] --------------------",
              src_ip.c_str(),
              src_port);
            if (!ptr_handler)
            {
                SD_MANAGER_LOGGER::error("Handler is null in recv_callback");
                return;
            }
            lttng_ust_tracepoint(someip, package_received, msg.get_length(), src_ip.c_str(), src_port);

            SdEndpointInfo endpoint(src_ip, src_port);
            ptr_handler->update_receivers_sid(endpoint);

            bool reboot_detected = ptr_handler->handle_endpoint_reboot(endpoint, msg.get_reboot_flag());
            if (reboot_detected)
                return;

            if (msg.get_entries().empty())
            {
                SD_MANAGER_LOGGER::error("Received message has no entries");
                return;
            }

            auto entries = msg.get_entries();

            for (const auto &ref_entry : entries)
            {
                if (ref_entry == nullptr)
                {
                    SD_MANAGER_LOGGER::error("Received entry is null");
                    continue;
                }

                switch (ref_entry->get_type())
                {
                case SdEntryType::FIND_SERVICE:
                {
                    SD_MANAGER_LOGGER::debug("RECEIVED FIND_SERVICE message");

                    auto sprt_serv_entry = (ref_entry && is_service_entry(ref_entry->get_type())) ? someIp::pool_static_pointer_cast<ServiceEntry>(ref_entry) : someIp::PoolPtr<ServiceEntry>();
                    if (sprt_serv_entry)
                    {
                        lttng_ust_tracepoint(someip, find_service_received,
                                             sprt_serv_entry->get_service_info()._service_id,
                                             sprt_serv_entry->get_service_info()._instance_id,
                                             sprt_serv_entry->get_service_info()._major_version,
                                             sprt_serv_entry->get_service_info()._minor_version);
                        ptr_handler->handle_find_service(*sprt_serv_entry);
                    }
                    else
                    {
                        SD_MANAGER_LOGGER::error("Received FIND_SERVICE message but entry is not a ServiceEntry");
                    }
                    break;
                }
                case SdEntryType::OFFER_SERVICE:
                {
                    SD_MANAGER_LOGGER::debug("Received OFFER_SERVICE message");
                    auto sprt_serv_entry = (ref_entry && is_service_entry(ref_entry->get_type())) ? someIp::pool_static_pointer_cast<ServiceEntry>(ref_entry) : someIp::PoolPtr<ServiceEntry>();
                    if (sprt_serv_entry)
                    {
                        lttng_ust_tracepoint(someip, offer_service_received,
                                             sprt_serv_entry->get_service_info()._service_id,
                                             sprt_serv_entry->get_service_info()._instance_id,
                                             sprt_serv_entry->get_service_info()._major_version,
                                             sprt_serv_entry->get_service_info()._minor_version,
                                             sprt_serv_entry->get_service_info()._ttl);
                        ptr_handler->handle_offer_service(sprt_serv_entry->get_service_info(), endpoint, msg.get_entry_options(sprt_serv_entry));
                    }
                    else
                    {
                        SD_MANAGER_LOGGER::error("Received OFFER_SERVICE message but entry is not a ServiceEntry");
                    }
                    break;
                }
                case SdEntryType::SUBSCRIBE_EVENTGROUP:
                {
                    SD_MANAGER_LOGGER::debug("Received SUBSCRIBE_EVENTGROUP message");
                    auto sprt_event_entry = (ref_entry && is_eventgroup_entry(ref_entry->get_type())) ? someIp::pool_static_pointer_cast<EventgroupEntry>(ref_entry) : someIp::PoolPtr<EventgroupEntry>();
                    if (sprt_event_entry)
                    {
                        lttng_ust_tracepoint(someip, eventgroup_subscribe_received,
                                             sprt_event_entry->get_eventgroup_info()._service_id,
                                             sprt_event_entry->get_eventgroup_info()._instance_id,
                                             sprt_event_entry->get_eventgroup_info()._major_version,
                                             sprt_event_entry->get_eventgroup_info()._eventgroup_id,
                                             sprt_event_entry->get_eventgroup_info()._ttl);
                        sprt_event_entry->update_options(msg.get_entry_options(sprt_event_entry));
                        ptr_handler->handle_subscribe_eventgroup(sprt_event_entry, endpoint);
                    }
                    else
                    {
                        SD_MANAGER_LOGGER::error("Received SUBSCRIBE_EVENTGROUP message but entry is not an EventgroupEntry");
                    }
                    break;
                }
                case SdEntryType::SUBSCRIBE_EVENTGROUP_ACK:
                {
                    auto sprt_event_entry = (ref_entry && is_eventgroup_entry(ref_entry->get_type())) ? someIp::pool_static_pointer_cast<EventgroupEntry>(ref_entry) : someIp::PoolPtr<EventgroupEntry>();
                    if (sprt_event_entry)
                    {
                        if (ptr_handler->m_subscriptions.handle_subscribe_ack(sprt_event_entry))
                        {
                            return;
                        }
                    }
                    break;
                }
                }
            }
            SD_MANAGER_LOGGER::debug_sd(
                "<<<<<<<<<<<<<<<<<<<<<<<< NUMBER OF SERVICES: [%zu] >>>>>>>>>>>>>>>>>>>>>>>",
                ptr_handler->m_registry.remote_count());
            // for (const auto &service : handler->_remote_services)
            // {
            //     if (service)
            //     { // Add null check
            //         auto endpoint_ip = service->get_endpoint()._ip;
            //         auto sender_ip = service->get_sender_info()._ip;

            //         ss << "----------------------------------------------------------------------------" << std::endl;
            //         ss << "ID: " << service->get_service_id() << " --> "
            //            << "Endpoint: " << ip_ntoa(&endpoint_ip) << ":" << service->get_endpoint()._port
            //            << " || Sender Endpoint: " << ip_ntoa(&sender_ip) << ":"
            //            << service->get_sender_info()._port << std::endl;
            //     }
            // }
            SD_MANAGER_LOGGER::debug_sd(
                "----------------------------------------------------------------------------\n");
        }

const someIp::PoolPtr<SdService> SdManager::find_local_service(const SdServiceInfo &ref_service_info) const {
  return m_registry.find_local_service(ref_service_info);
}

const Event* SdManager::find_local_event(const SdServiceInfo &ref_service_info, uint16_t event_id) const {
  return m_registry.find_local_event(ref_service_info, event_id);
}

Eventgroup* SdManager::find_local_eventgroup(const SdServiceInfo &ref_service_info, uint16_t eventgroup_id) const {
  return m_registry.find_local_eventgroup(ref_service_info, eventgroup_id);
}

const someIp::PoolPtr<SdService> SdManager::find_service(const SdServiceInfo &ref_service_info){
  auto sprt_local_service = m_registry.find_local_service(ref_service_info);
  if (sprt_local_service)
    return sprt_local_service;

  auto sprt_remote_service = m_registry.find_remote(ref_service_info);
  if (sprt_remote_service)
    return sprt_remote_service;

  send_find_service(ref_service_info);
  someIp::os::sleep_ms(100); // TODO: replace magic number
  return m_registry.find_remote(ref_service_info);
}

SdManager::~SdManager()
{
  unsubscribe_all();
  stop_all_services();
  // stop timer callbacks before their components are destroyed
  m_event_manager.stop();
}

} // namespace sd
} // namespace someIp
