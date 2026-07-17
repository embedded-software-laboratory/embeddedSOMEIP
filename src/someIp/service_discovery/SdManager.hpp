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

// coordinator of the SOME/IP Service Discovery protocol
#ifndef SOMEIP_SD_MANAGER_HPP_
#define SOMEIP_SD_MANAGER_HPP_

#include <cstdint>
#include "someIp/utils/PoolPtr.hpp"
#include <vector>
#include <memory>

#include "Def.hpp"
#include "SdService.hpp"
#include "ServiceEntry.hpp"
#include "Option.hpp"
#include "someIp/net/IpAddress.hpp"
#include "someIp/ThreadPool.hpp"
#include "someIp/utils/InplaceFunction.hpp"
#include "someIp/utils/TimerScheduler.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "SdClientService.hpp"
#include "SdSessionManager.hpp"
#include "SdServiceRegistry.hpp"
#include "SdSubscriptionManager.hpp"
#include "SdFindManager.hpp"
#include "SdOfferManager.hpp"
#include "SdTransport.hpp"

#include "someip_tp.h"

namespace someIp
{
namespace sd
{
class ConfigurationOption;

// lets the owner send current field values to a freshly ACKed subscriber
using NewSubscriberHook = someIp::InplaceFunction<void(const SdServiceInfo &, uint16_t, const SdSubscriberInfo &)>;

class SdManager : public SdTransport
{
private:
  // declared before the components that hold a reference to it
  TimerScheduler m_event_manager;
  SdSessionManager m_sessions;
  SdServiceRegistry m_registry;
  SdSubscriptionManager m_subscriptions;
  SdFindManager m_find;
  SdOfferManager m_offer;

  ThreadPool *m_ptr_thread_pool;
  IpAddr m_local_ip;
  IpAddr m_multicast_ip;

  SdInterfaceStatus m_link_status = SdInterfaceStatus::LINK_DOWN;

  // empty unless the owner opted into initial field notifications
  NewSubscriberHook m_on_new_subscriber;

  // send a single OFFER_SERVICE in direct answer to a FIND_SERVICE
  bool _offer_service(someIp::PoolPtr<SdService> sprt_service,
                    const IpAddr &ref_dest_ip,
                    uint16_t dest_port);

public:
  SdManager(ThreadPool *ptr_thread_pool, IpAddr local_ip, const ServiceVec &ref_services = {});
  ~SdManager();
  /* client_endpoint functions */
  bool send_find_service(const SdServiceInfo &ref_service_info,
                        someIp::PoolPtr<ConfigurationOption> sprt_config_option = nullptr);

  void subscribe(const SdEventgroupInfo &ref_eventgroup, OptionVec options = {});
  void unsubscribe_all();

  /* service_endpoint functions */
  bool offer_service(someIp::PoolPtr<SdService> sprt_service,
                    const IpAddr &ref_src_ip,
                    uint16_t src_port,
                    OptionVec options = {});
  bool stop_offer_service(SdServiceInfo service_info);
  void stop_all_services();

  void register_service(someIp::PoolPtr<SdService> sprt_service);

  void unregister_service(someIp::PoolPtr<SdService> sprt_service);

  void register_client_service(someIp::PoolPtr<SdClientService> sprt_client);

  /* common functions */

  void on_ttl_expire(SdServiceInfo service_info, const SdEndpointInfo &ref_endpoint, const SdEndpointInfo &ref_sedner_endpoint);

  bool find_local_port(uint16_t port);
  void remove_remote_subscription(const SdServiceInfo &ref_info);

  bool send_package(SdMessage msg, IpAddr dest_ip, uint16_t dest_port, uint16_t src_port = config::SD_PORT) override;

  void handle_offer_service(const SdServiceInfo &ref_service, const SdEndpointInfo &ref_sender_endpoint, const OptionVec &ref_options);
  void handle_find_service(const ServiceEntry &ref_entry);
  void handle_subscribe_eventgroup(someIp::PoolPtr<EventgroupEntry> sprt_entry,
                                  const SdEndpointInfo &ref_sender);
  void set_new_subscriber_hook(NewSubscriberHook cb) { m_on_new_subscriber = std::move(cb); }
  uint16_t update_senders_sid(const SdEndpointInfo &ref_endpoint);
  uint16_t update_receivers_sid(const SdEndpointInfo &ref_endpoint);

  bool offer_local_services();
  bool send_eventgroup_ack(const SdEndpointInfo &ref_dest_endpoint,
                          const SdEventgroupInfo &ref_eventgroup,
                          someIp::PoolPtr<Ipv4Option> sprt_ipv4_multicast_option = nullptr,
                          someIp::PoolPtr<Ipv6Option> sprt_ipv6_multicast_option = nullptr);

  bool handle_endpoint_reboot(const SdEndpointInfo &ref_endpoint, bool new_reboot_flag);
  bool validate_offer_service(const SdServiceInfo &ref_service_info);

  void validate_offer_service_options(OptionVec &ref_valid_options, const OptionVec &ref_options);
  bool validate_eventgroup_options(someIp::PoolPtr<EventgroupEntry> sprt_entry);
  bool get_stop_flag() { return m_subscriptions.get_stop_flag(); };

  static void recv_callback(SdManager *ptr_handler, SdMessage msg, IpAddr src_ip, uint16_t src_port);


  const someIp::PoolPtr<SdService> find_local_service(const SdServiceInfo &ref_service_info) const;

  const Event* find_local_event(const SdServiceInfo &ref_service_info, const uint16_t event_id) const;

  Eventgroup* find_local_eventgroup(const SdServiceInfo &ref_service_info, const uint16_t eventgroup_id) const;

  // schedule work on the SD scheduler instead of spawning ad-hoc threads
  someIp::PoolPtr<TimerEvent> schedule_periodic(const char *ptr_name, int milliseconds, TimerCallback cb) {
    return m_event_manager.add_timer(ptr_name, milliseconds, std::move(cb), true);
  }
  void cancel_timer(const someIp::PoolPtr<TimerEvent> &ref_timer) {
    if (ref_timer) m_event_manager.disable_timer(ref_timer);
  }

  // check local then remote, else send FIND and wait
  const someIp::PoolPtr<SdService> find_service(const SdServiceInfo &ref_service_info);
};

} // sd
} // namespace someIp

#endif // SOMEIP_SD_MANAGER_HPP_
