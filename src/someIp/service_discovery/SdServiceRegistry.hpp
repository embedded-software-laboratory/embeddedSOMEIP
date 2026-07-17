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

// owns the SD service lists with thread-safe access
#ifndef SOMEIP_SD_SERVICE_REGISTRY_HPP_
#define SOMEIP_SD_SERVICE_REGISTRY_HPP_

#include <memory>
#include "someIp/utils/PoolPtr.hpp"
#include <unordered_map>
#include <utility>
#include <vector>

#include "Def.hpp"
#include "SdService.hpp"
#include "SdSessionManager.hpp" // std::hash<SdEndpointInfo>
#include "someIp/os/Os.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/utils/StaticMap.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp
{
    namespace sd
    {
        // bounded, no-heap service lists (one cap covers all four lists)
        using ServiceVec = StaticVector<someIp::PoolPtr<SdService>, config::MAX_REMOTE_SERVICES>;
        using PortVec = StaticVector<uint16_t, config::MAX_LOCAL_SERVICES>;
        // remote services grouped by sending endpoint (reboot handling)
        using EndpointServiceVec = StaticVector<someIp::PoolPtr<SdService>, config::MAX_SERVICES_PER_ENDPOINT>;
        using EndpointMap = StaticMap<SdEndpointInfo, EndpointServiceVec, config::MAX_REMOTE_SERVICES>;

        class SdServiceRegistry
        {
        public:
            explicit SdServiceRegistry(const ServiceVec &ref_local_services);
            ~SdServiceRegistry();

            SdServiceRegistry(const SdServiceRegistry &) = delete;
            SdServiceRegistry &operator=(const SdServiceRegistry &) = delete;

            void register_service(someIp::PoolPtr<SdService> sprt_service);
            void unregister_service(someIp::PoolPtr<SdService> sprt_service);

            const someIp::PoolPtr<SdService> find_local_service(const SdServiceInfo &ref_service_info) const;
            const Event *find_local_event(const SdServiceInfo &ref_service_info, uint16_t event_id) const;
            Eventgroup *find_local_eventgroup(const SdServiceInfo &ref_service_info, uint16_t eventgroup_id) const;
            bool find_local_port(uint16_t port) const;

            // snapshot of the registered local services
            ServiceVec local_services() const;

            // admission check before offering, port valid and free
            bool validate_offer_service(const SdServiceInfo &ref_service_info) const;

            void add_offered(someIp::PoolPtr<SdService> sprt_service);
            void add_separate(someIp::PoolPtr<SdService> sprt_service);
            void clear_offered();
            std::size_t offered_count() const;

            // snapshot of the offered services list
            ServiceVec offered_services() const;

            // service offered (either list) matching the eventgroup, or nullptr
            someIp::PoolPtr<SdService> find_separate_by_eventgroup(const SdEventgroupInfo &ref_eventgroup_info) const;
            someIp::PoolPtr<SdService> find_offered_by_eventgroup(const SdEventgroupInfo &ref_eventgroup_info) const;

            // remove from offered or separate, service is null if not found
            std::pair<someIp::PoolPtr<SdService>, bool> remove_offered_or_separate(const SdServiceInfo &ref_service_info);

            // empty both offer lists and return their contents
            std::pair<ServiceVec, ServiceVec> take_offered_and_separate();

            someIp::PoolPtr<SdService> find_remote(const SdServiceInfo &ref_service_info) const;
            std::size_t remote_count() const;

            // add a discovered remote service, returns true if caller should create a TTL timer
            bool add_remote(someIp::PoolPtr<SdService> sprt_remote_service, const SdEndpointInfo &ref_sender_endpoint);

            // update a known remote, caller resets the timer
            someIp::PoolPtr<SdService> refresh_remote(const SdServiceInfo &ref_service_info,
                                                       const SdEndpointInfo &ref_endpoint,
                                                       const OptionVec &ref_options);

            // remove a remote service, returns it (caller disables timer) or nullptr
            someIp::PoolPtr<SdService> remove_remote(const SdServiceInfo &ref_service_info,
                                                      const SdEndpointInfo &ref_endpoint,
                                                      const SdEndpointInfo &ref_sender_endpoint);

            // remove all remote services from an endpoint, returns them (caller disables timers)
            ServiceVec purge_endpoint(const SdEndpointInfo &ref_endpoint);

        private:
            // unlocked helpers, caller holds the matching mutex
            ServiceVec::const_iterator
            _find_remote(const SdServiceInfo &ref_service_info, const SdEndpointInfo &ref_endpoint) const;
            void _erase_from_endpoint_map(const someIp::PoolPtr<SdService> &ref_service,
                                          const SdEndpointInfo &ref_sender_endpoint);

            ServiceVec m_local_services;
            ServiceVec m_remote_services;
            ServiceVec m_offered_services;
            ServiceVec m_separate_services;
            EndpointMap m_endpoints_services;
            PortVec m_local_ports;

            mutable os::Mutex m_local_mtx;   /* _local_services, _local_ports */
            mutable os::Mutex m_remote_mtx;  /* _remote_services, _endpoints_services */
            mutable os::Mutex m_offered_mtx; /* _offered_services, _separate_services */
        };

    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_SERVICE_REGISTRY_HPP_
