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

#include "SdServiceRegistry.hpp"
#include "someIp/utils/PoolPtr.hpp"

#include <algorithm>

#include "someIp/logging/BaseLogger.hpp"
#include "someIp/os/Os.hpp"

constexpr char SD_SERVICE_REGISTRY_TAG[] = "SD_SERVICE_REGISTRY";
using SD_REGISTRY_LOGGER = BaseLogger<SD_SERVICE_REGISTRY_TAG>;

namespace someIp
{
    namespace sd
    {

        SdServiceRegistry::SdServiceRegistry(const ServiceVec &ref_local_services)
            : m_local_services()
        {
            for (const auto &ref_s : ref_local_services) m_local_services.push_back(ref_s);
        }

        SdServiceRegistry::~SdServiceRegistry()
        {
        }

        void SdServiceRegistry::register_service(someIp::PoolPtr<SdService> sprt_service)
        {
            os::Guard lock(m_local_mtx);
            auto ptr_it = std::find_if(m_local_services.begin(), m_local_services.end(), [sprt_service](const auto it)
                                   { return it->get_info() == sprt_service->get_info(); });
            if (ptr_it != m_local_services.end())
            {
                SD_REGISTRY_LOGGER::error("Service is already registered");
            }
            m_local_services.push_back(sprt_service);
            SD_REGISTRY_LOGGER::debug("Service is registered: id: %d, instance: %d, version: %d.%d",
                                      sprt_service->get_id(),
                                      sprt_service->get_instance_id(),
                                      sprt_service->get_major_version(),
                                      sprt_service->get_minor_version());
        }

        void SdServiceRegistry::unregister_service(someIp::PoolPtr<SdService> sprt_service)
        {
            os::Guard lock(m_local_mtx);
            auto ptr_it = std::remove_if(m_local_services.begin(), m_local_services.end(), [sprt_service](const auto it)
                                     { return it && it->get_info() == sprt_service->get_info(); });
            if (ptr_it == m_local_services.end())
            {
                SD_REGISTRY_LOGGER::error("Service is not registered");
                return;
            }
            m_local_services.erase(ptr_it, m_local_services.end());
        }

        const someIp::PoolPtr<SdService> SdServiceRegistry::find_local_service(const SdServiceInfo &ref_service_info) const
        {
            os::Guard lock(m_local_mtx);
            for (const auto &ref_service : m_local_services)
            {
                if (ref_service->get_info() == ref_service_info)
                    return ref_service;
            }
            SD_REGISTRY_LOGGER::log(
                "No local service found for: ID = %u, Instance ID = %u, Major = %d, Minor = %u",
                ref_service_info._service_id,
                ref_service_info._instance_id,
                static_cast<int>(ref_service_info._major_version),
                ref_service_info._minor_version);
            return nullptr;
        }

        const Event *SdServiceRegistry::find_local_event(const SdServiceInfo &ref_service_info, uint16_t event_id) const
        {
            auto sprt_service = find_local_service(ref_service_info);
            if (!sprt_service)
                return nullptr;

            const Event *ptr_event = sprt_service->get_event(event_id);
            if (ptr_event)
                return ptr_event;

            SD_REGISTRY_LOGGER::error("No local event found for: Service ID = %u, Event ID = %u\n", ref_service_info._service_id, event_id);
            return nullptr;
        }

        Eventgroup *SdServiceRegistry::find_local_eventgroup(const SdServiceInfo &ref_service_info, uint16_t eventgroup_id) const
        {
            auto sprt_service = find_local_service(ref_service_info);
            if (!sprt_service)
                return nullptr;

            Eventgroup *ptr_eventgroup = sprt_service->get_eventgroup(eventgroup_id);
            if (ptr_eventgroup)
                return ptr_eventgroup;

            SD_REGISTRY_LOGGER::error("No local eventgroup found for: Service ID = %u, Eventgroup ID = %u\n", ref_service_info._service_id, eventgroup_id);
            return nullptr;
        }

        bool SdServiceRegistry::find_local_port(uint16_t port) const
        {
            os::Guard lock(m_local_mtx);
            return std::find(m_local_ports.begin(), m_local_ports.end(), port) != m_local_ports.end();
        }

        ServiceVec SdServiceRegistry::local_services() const
        {
            os::Guard lock(m_local_mtx);
            return m_local_services;
        }

        bool SdServiceRegistry::validate_offer_service(const SdServiceInfo &ref_service_info) const
        {
            if (ref_service_info._src_port == 0)
            {
                SD_REGISTRY_LOGGER::error("invalid source port");
                return false;
            }

            os::Guard lock(m_offered_mtx);
            if (m_offered_services.empty())
            {
                return true;
            }

            // reject port reuse across different instances of same service/version
            for (const auto &ref_offered_service : m_offered_services)
            {
                auto existing_info = ref_offered_service->get_info();
                if (existing_info == ref_service_info)
                {
                    SD_REGISTRY_LOGGER::error("Service is already offered with this info, going to be ignored");
                    return false;
                }
                if (existing_info._src_port == ref_service_info._src_port &&
                    existing_info._service_id == ref_service_info._service_id &&
                    existing_info._major_version == ref_service_info._major_version &&
                    existing_info._minor_version == ref_service_info._minor_version &&
                    existing_info._instance_id != ref_service_info._instance_id)
                {
                    SD_REGISTRY_LOGGER::error("Port conflict: Same service ID and version offered on same port with different instance ID");
                    return false;
                }
            }

            return true;
        }

        void SdServiceRegistry::add_offered(someIp::PoolPtr<SdService> sprt_service)
        {
            os::Guard lock(m_offered_mtx);
            m_offered_services.push_back(sprt_service);
        }

        void SdServiceRegistry::add_separate(someIp::PoolPtr<SdService> sprt_service)
        {
            os::Guard lock(m_offered_mtx);
            m_separate_services.push_back(sprt_service);
        }

        void SdServiceRegistry::clear_offered()
        {
            os::Guard lock(m_offered_mtx);
            m_offered_services.clear();
        }

        std::size_t SdServiceRegistry::offered_count() const
        {
            os::Guard lock(m_offered_mtx);
            return m_offered_services.size();
        }

        ServiceVec SdServiceRegistry::offered_services() const
        {
            os::Guard lock(m_offered_mtx);
            return m_offered_services;
        }

        namespace
        {
            bool matches_eventgroup(const someIp::PoolPtr<SdService> &ref_service,
                                    const SdEventgroupInfo &ref_eventgroup_info)
            {
                const auto &ref_info = ref_service->get_info();

                if (ref_info._service_id != ref_eventgroup_info._service_id ||
                    ref_info._instance_id != ref_eventgroup_info._instance_id ||
                    ref_info._major_version != ref_eventgroup_info._major_version)
                    return false;
                return ref_service->find_eventgroup(ref_eventgroup_info._eventgroup_id);
            }
        } // namespace

        someIp::PoolPtr<SdService> SdServiceRegistry::find_separate_by_eventgroup(const SdEventgroupInfo &ref_eventgroup_info) const
        {
            os::Guard lock(m_offered_mtx);
            auto ptr_it = std::find_if(m_separate_services.begin(), m_separate_services.end(),
                                   [&ref_eventgroup_info](const auto &ref_service)
                                   { return matches_eventgroup(ref_service, ref_eventgroup_info); });
            return ptr_it != m_separate_services.end() ? *ptr_it : nullptr;
        }

        someIp::PoolPtr<SdService> SdServiceRegistry::find_offered_by_eventgroup(const SdEventgroupInfo &ref_eventgroup_info) const
        {
            os::Guard lock(m_offered_mtx);
            auto ptr_it = std::find_if(m_offered_services.begin(), m_offered_services.end(),
                                   [&ref_eventgroup_info](const auto &ref_service)
                                   { return matches_eventgroup(ref_service, ref_eventgroup_info); });
            return ptr_it != m_offered_services.end() ? *ptr_it : nullptr;
        }

        std::pair<someIp::PoolPtr<SdService>, bool>
        SdServiceRegistry::remove_offered_or_separate(const SdServiceInfo &ref_service_info)
        {
            os::Guard lock(m_offered_mtx);
            auto ptr_it = std::find_if(m_offered_services.begin(), m_offered_services.end(),
                                   [&ref_service_info](const auto &ref_service)
                                   { return ref_service->get_info() == ref_service_info; });
            if (ptr_it != m_offered_services.end())
            {
                auto sprt_service = *ptr_it;
                m_offered_services.erase(ptr_it);
                return {sprt_service, true};
            }

            ptr_it = std::find_if(m_separate_services.begin(), m_separate_services.end(),
                              [&ref_service_info](const auto &ref_service)
                              { return ref_service->get_info() == ref_service_info; });
            if (ptr_it != m_separate_services.end())
            {
                auto sprt_service = *ptr_it;
                m_separate_services.erase(ptr_it);
                return {sprt_service, false};
            }

            return {nullptr, false};
        }

        std::pair<ServiceVec, ServiceVec>
        SdServiceRegistry::take_offered_and_separate()
        {
            os::Guard lock(m_offered_mtx);
            auto sprt_result = std::make_pair(std::move(m_offered_services), std::move(m_separate_services));
            m_offered_services.clear();
            m_separate_services.clear();
            return sprt_result;
        }

        ServiceVec::const_iterator
        SdServiceRegistry::_find_remote(const SdServiceInfo &ref_service_info, const SdEndpointInfo &ref_endpoint) const
        {
            return std::find_if(m_remote_services.begin(), m_remote_services.end(),
                                [&ref_service_info, &ref_endpoint](const someIp::PoolPtr<SdService> &ref_service)
                                {
                                    return ref_service->get_info() == ref_service_info && ref_service->get_endpoint() == ref_endpoint;
                                });
        }

        void SdServiceRegistry::_erase_from_endpoint_map(const someIp::PoolPtr<SdService> &ref_service,
                                                           const SdEndpointInfo &ref_sender_endpoint)
        {
            auto ptr_endpoint_it = m_endpoints_services.find(ref_sender_endpoint);
            if (ptr_endpoint_it == m_endpoints_services.end())
                return;

            auto &ref_services = ptr_endpoint_it->second;
            ref_services.erase(std::remove(ref_services.begin(), ref_services.end(), ref_service), ref_services.end());

            if (ref_services.empty())
            {
                m_endpoints_services.erase(ptr_endpoint_it);
            }
        }

        someIp::PoolPtr<SdService> SdServiceRegistry::find_remote(const SdServiceInfo &ref_service_info) const
        {
            os::Guard lock(m_remote_mtx);
            auto ptr_it = std::find_if(m_remote_services.begin(), m_remote_services.end(),
                                   [&ref_service_info](const someIp::PoolPtr<SdService> &ref_service)
                                   { return ref_service->get_info() == ref_service_info; });
            return ptr_it != m_remote_services.end() ? *ptr_it : nullptr;
        }

        std::size_t SdServiceRegistry::remote_count() const
        {
            os::Guard lock(m_remote_mtx);
            return m_remote_services.size();
        }

        bool SdServiceRegistry::add_remote(someIp::PoolPtr<SdService> sprt_remote_service, const SdEndpointInfo &ref_sender_endpoint)
        {
            os::Guard lock(m_remote_mtx);
            m_remote_services.push_back(sprt_remote_service);
            m_endpoints_services[ref_sender_endpoint].push_back(sprt_remote_service);
            // TTL 0xFFFFFF means valid until reboot, no timer needed
            return sprt_remote_service->get_ttl() != SD_MAX_TTL;
        }

        someIp::PoolPtr<SdService> SdServiceRegistry::refresh_remote(const SdServiceInfo &ref_service_info,
                                                                        const SdEndpointInfo &ref_endpoint,
                                                                        const OptionVec &ref_options)
        {
            os::Guard lock(m_remote_mtx);
            auto ptr_it = _find_remote(ref_service_info, ref_endpoint);
            if (ptr_it == m_remote_services.end())
                return nullptr;

            (*ptr_it)->set_ttl(ref_service_info._ttl);
            (*ptr_it)->update_options(ref_options);
            return *ptr_it;
        }

        someIp::PoolPtr<SdService> SdServiceRegistry::remove_remote(const SdServiceInfo &ref_service_info,
                                                                       const SdEndpointInfo &ref_endpoint,
                                                                       const SdEndpointInfo &ref_sender_endpoint)
        {
            os::Guard lock(m_remote_mtx);
            auto ptr_it = _find_remote(ref_service_info, ref_endpoint);
            if (ptr_it == m_remote_services.end())
                return nullptr;

            auto sprt_service = *ptr_it;
            m_remote_services.erase(ptr_it);
            _erase_from_endpoint_map(sprt_service, ref_sender_endpoint);
            return sprt_service;
        }

        ServiceVec SdServiceRegistry::purge_endpoint(const SdEndpointInfo &ref_endpoint)
        {
            os::Guard lock(m_remote_mtx);
            auto ptr_endpoint_it = m_endpoints_services.find(ref_endpoint);
            if (ptr_endpoint_it == m_endpoints_services.end())
            {
                return {};
            }

            // copy to avoid iterator invalidation
            EndpointServiceVec services_to_remove = ptr_endpoint_it->second;

            for (const auto &ref_service : services_to_remove)
            {
                m_remote_services.erase(
                    std::remove(m_remote_services.begin(), m_remote_services.end(), ref_service),
                    m_remote_services.end());
            }

            m_endpoints_services.erase(ptr_endpoint_it);

            ServiceVec result;
            for (const auto &ref_s : services_to_remove) result.push_back(ref_s);
            return result;
        }

    } // namespace sd
} // namespace someIp
