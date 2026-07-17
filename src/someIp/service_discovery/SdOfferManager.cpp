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
#include "SdOfferManager.hpp"

#include <algorithm>
#include <random>

#include "ServiceEntry.hpp"
#include "SdMessage.hpp"
#include "Ipv4Option.hpp"
#include "Ipv6Option.hpp"
#include "someIp/config/config.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/os/Os.hpp"

#include "someip_tp.h"

constexpr char SD_OFFER_MANAGER_TAG[] = "SD_OFFER_MANAGER";
using SD_OFFER_LOGGER = BaseLogger<SD_OFFER_MANAGER_TAG>;

namespace someIp
{
    namespace sd
    {

        using S = OfferState;
        using E = OfferEvent;
        using A = SdOfferFsm;

        const SdOfferFsm::Machine::StateDef SdOfferFsm::m_kStates[static_cast<std::size_t>(S::COUNT)] = {
            /* NOT_READY    */ {0, E::TIMEOUT},
            /* INITIAL_WAIT */ {Machine::DYNAMIC_TIMEOUT, E::TIMEOUT},
            /* REPETITION   */ {Machine::DYNAMIC_TIMEOUT, E::TIMEOUT},
            /* MAIN         */ {config::CYCLIC_OFFER_DELAY ? config::CYCLIC_OFFER_DELAY * 10 : 0, E::TIMEOUT},
            /* STOPPED      */ {0, E::TIMEOUT},
        };

        const SdOfferFsm::Machine::Transition SdOfferFsm::m_kTransitions[] = {
            {S::NOT_READY, E::START, S::INITIAL_WAIT, &A::prepare_initial_wait},
            {S::INITIAL_WAIT, E::TIMEOUT, S::REPETITION, &A::on_enter_repetition},
            {S::REPETITION, E::TIMEOUT, S::REPETITION, &A::on_repetition_tick},
            {S::REPETITION, E::REPETITIONS_DONE, S::MAIN, &A::on_enter_main},
            {S::MAIN, E::TIMEOUT, S::MAIN, &A::on_cyclic_tick},
            // ABORT only halts cadence, STOP_OFFER is sent by SdOfferManager
            {S::INITIAL_WAIT, E::ABORT, S::STOPPED, nullptr},
            {S::REPETITION, E::ABORT, S::STOPPED, nullptr},
            {S::MAIN, E::ABORT, S::STOPPED, nullptr},
        };

        const std::size_t SdOfferFsm::m_kNumTransitions = sizeof(SdOfferFsm::m_kTransitions) / sizeof(SdOfferFsm::m_kTransitions[0]);

        SdOfferFsm::SdOfferFsm(someIp::PoolPtr<SdService> sprt_service,
                                   SdTransport &ref_transport,
                                   TimerScheduler &ref_timers)
            : m_sprt_service(std::move(sprt_service)),
              m_ref_transport(ref_transport),
              m_fsm(S::NOT_READY, m_kStates, static_cast<std::size_t>(S::COUNT),
                   m_kTransitions, m_kNumTransitions, this, ref_timers,
                   fsm_name(m_name_buf, sizeof(m_name_buf), "OFFER_SERVICE_", m_sprt_service->get_info()._service_id))
        {
            IpAddr::from_string(config::SD_MULTICAST_IP, m_multicast_ip);
            m_fsm.setTimeoutProvider(&A::timeout_for);
            m_fsm.setObserver(&A::observe);
            m_fsm.start();
        }

        void SdOfferFsm::prepare_initial_wait()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(config::INITIAL_WAIT_MIN_MS, config::INITIAL_WAIT_MAX_MS);
            m_initial_delay_ms = dis(gen);

            lttng_ust_tracepoint(someip, offer_service_initial_phase_started,
                                 m_sprt_service->get_info()._service_id,
                                 m_sprt_service->get_info()._instance_id,
                                 m_sprt_service->get_info()._major_version,
                                 m_sprt_service->get_info()._minor_version,
                                 m_sprt_service->get_port(),
                                 m_initial_delay_ms);
        }

        void SdOfferFsm::on_enter_repetition()
        {
            lttng_ust_tracepoint(someip, offer_service_repetition_phase_started,
                                 m_sprt_service->get_info()._service_id,
                                 m_sprt_service->get_info()._instance_id,
                                 m_sprt_service->get_info()._major_version,
                                 m_sprt_service->get_info()._minor_version,
                                 m_sprt_service->get_port());

            send_offer();

            // [PRS_SOMEIPSD_00409] if REPETITIONS_MAX is 0, skip Repetition Phase
            if (config::REPETITIONS_MAX == 0 ||
                m_sprt_service->get_repetition_count() >= config::REPETITIONS_MAX ||
                m_sprt_service->get_total_delay().count() >= config::MAX_REPETITION_DELAY)
            {
                m_fsm.postEvent(E::REPETITIONS_DONE);
                return;
            }

            m_sprt_service->add_to_total_delay(m_sprt_service->get_current_delay());
        }

        void SdOfferFsm::on_repetition_tick()
        {
            send_offer();
            m_sprt_service->increment_repetition();
            // [PRS_SOMEIPSD_00406] delay doubles after each repetition message
            m_sprt_service->double_delay();

            // [PRS_SOMEIPSD_00407] up to REPETITIONS_MAX entries in Repetition Phase
            if (m_sprt_service->get_repetition_count() >= config::REPETITIONS_MAX ||
                m_sprt_service->get_total_delay().count() >= config::MAX_REPETITION_DELAY)
            {
                m_fsm.postEvent(E::REPETITIONS_DONE);
                return;
            }

            m_sprt_service->add_to_total_delay(m_sprt_service->get_current_delay());
        }

        void SdOfferFsm::on_enter_main()
        {
            SD_OFFER_LOGGER::debug_sd("############################################################################");
            SD_OFFER_LOGGER::debug_sd("Service %u - Entering Main Phase", m_sprt_service->get_info()._service_id);
            SD_OFFER_LOGGER::debug_sd("############################################################################");

            lttng_ust_tracepoint(someip, offer_service_main_phase_started,
                                 m_sprt_service->get_info()._service_id,
                                 m_sprt_service->get_info()._instance_id,
                                 m_sprt_service->get_info()._major_version,
                                 m_sprt_service->get_info()._minor_version,
                                 m_sprt_service->get_port());

            if (!config::CYCLIC_OFFER_DELAY)
            {
                return;
            }

            send_offer();
        }

        void SdOfferFsm::on_cyclic_tick()
        {
            send_offer();
        }

        void SdOfferFsm::send_offer()
        {
            SdMessage message;
            auto sprt_entry = pool_make<ServiceEntry>(SdEntryType::OFFER_SERVICE, m_sprt_service->get_info());
            message.add_entry(sprt_entry);

            auto options = m_sprt_service->get_options();
            message.overwrite_entry_options(sprt_entry, SD_FIRST_OPTION_RUN, options);

            lttng_ust_tracepoint(someip, offer_service_sent,
                                 m_sprt_service->get_info()._service_id,
                                 m_sprt_service->get_info()._instance_id,
                                 m_sprt_service->get_info()._major_version,
                                 m_sprt_service->get_info()._minor_version,
                                 m_sprt_service->get_info()._ttl,
                                 m_sprt_service->get_port());
            m_ref_transport.send_package(message, m_multicast_ip, config::SD_PORT, config::SD_PORT);
        }

        long long SdOfferFsm::timeout_for(OfferState entered)
        {
            switch (entered)
            {
            case S::INITIAL_WAIT:
                return m_initial_delay_ms;
            case S::REPETITION:
                // wait the current (not yet doubled) delay before next repetition
                return static_cast<long long>(m_sprt_service->get_current_delay().count());
            default:
                return 0;
            }
        }

        void SdOfferFsm::observe(OfferState from, OfferEvent event, OfferState to)
        {
            (void)from;
            (void)event;
            switch (to)
            {
            case S::INITIAL_WAIT:
                m_sprt_service->set_status(SdPhase::INITIAL_WAIT);
                break;
            case S::REPETITION:
                m_sprt_service->set_status(SdPhase::REPETITION);
                break;
            case S::MAIN:
                m_sprt_service->set_status(SdPhase::MAIN);
                break;
            case S::STOPPED:
                m_sprt_service->set_status(SdPhase::NOT_READY);
                break;
            default:
                break;
            }
        }

        SdOfferManager::SdOfferManager(SdServiceRegistry &ref_registry, SdTransport &ref_transport, TimerScheduler &ref_timers)
            : m_ref_registry(ref_registry), m_ref_transport(ref_transport), m_ref_timers(ref_timers)
        {
            IpAddr::from_string(config::SD_MULTICAST_IP, m_multicast_ip);
        }

        SdOfferManager::~SdOfferManager()
        {
        }

        void SdOfferManager::_start_machine(someIp::PoolPtr<SdService> sprt_service)
        {
            SdOfferFsm *ptr_machine = nullptr;
            {
                os::Guard lock(m_mtx);
                m_machines.push_back(m_fsm_pool.acquire(std::move(sprt_service), m_ref_transport, m_ref_timers));
                ptr_machine = m_machines.back();
            }
            // post events outside the manager lock
            ptr_machine->start();
        }

        void SdOfferManager::_abort_machine(const someIp::PoolPtr<SdService> &ref_service)
        {
            SdOfferFsm *ptr_machine = nullptr;
            {
                os::Guard lock(m_mtx);
                // search backwards, re-offered service gets a fresh machine
                for (size_t i = m_machines.size(); i-- > 0;)
                {
                    if (m_machines[i]->service() == ref_service)
                    {
                        ptr_machine = m_machines[i];
                        break;
                    }
                }
            }
            if (ptr_machine != nullptr)
            {
                ptr_machine->abort();
            }
        }

        bool SdOfferManager::start_offer(someIp::PoolPtr<SdService> sprt_service,
                                           const IpAddr &ref_src_ip,
                                           uint16_t src_port,
                                           OptionVec options_)
        {
            if (!m_ref_registry.validate_offer_service(sprt_service->get_info()))
            {
                SD_OFFER_LOGGER::debug_sd(
                    "failed to validate offer service");
                return false;
            }

            if (sprt_service->get_ttl() == 0)
            {
                SD_OFFER_LOGGER::error("TTL can't be 0, as it will be interpreted as stop offer service");
                return false;
            }
            if constexpr (CYCLIC_OFFER_DELAY_DEFINE)
            {
                if (sprt_service->get_ttl() < config::CYCLIC_OFFER_DELAY)
                {
                    SD_OFFER_LOGGER::error("TTL can't be less than CYCLIC_OFFER_DELAY: %d ms", config::CYCLIC_OFFER_DELAY);
                    return false;
                }
            }

            someIp::PoolPtr<Option> sprt_endpoint_option;
#if LWIP_IPV6
            if (IP_IS_V6_VAL(src_ip))
            {
                const ip6_addr_t *ipv6 = ip_2_ip6(&src_ip);
                endpoint_option = pool_make<Ipv6Option>(ipv6, src_port);
            }
            else
            {
                SD_OFFER_LOGGER::error("Source IP is not IPv6");
                return false;
            }
#else
            // IpAddr is always IPv4 in this build
            sprt_endpoint_option = pool_make<Ipv4Option>(ref_src_ip, src_port);
#endif

            OptionVec msg_options;
            msg_options.push_back(sprt_endpoint_option);

            if (!options_.empty())
            {
                validate_offer_service_options(msg_options, options_);
            }
            sprt_service->update_options(msg_options);

            sprt_service->set_status(SdPhase::READY);
            m_ref_registry.add_separate(sprt_service);

            _start_machine(sprt_service);

            return true;
        }

        bool SdOfferManager::offer_local_services(const IpAddr &ref_local_ip)
        {
            m_ref_registry.clear_offered();

            ServiceVec prepared_services;

            for (auto &ref_service : m_ref_registry.local_services())
            {
                if (ref_service->get_ttl() == 0)
                {
                    SD_OFFER_LOGGER::error("TTL can't be 0 for service: %d, service going to be ignored", ref_service->get_id());
                    continue;
                }
                if (!m_ref_registry.validate_offer_service(ref_service->get_info()))
                {
                    continue;
                }

                if constexpr (CYCLIC_OFFER_DELAY_DEFINE)
                {
                    if (ref_service->get_ttl() < config::CYCLIC_OFFER_DELAY)
                    {
                        SD_OFFER_LOGGER::error("TTL can't be less than CYCLIC_OFFER_DELAY: %d ms for service: %d, going to be reset to CYCLIC_OFFER_DELAY: %d",
                                               config::CYCLIC_OFFER_DELAY, ref_service->get_id(), config::CYCLIC_OFFER_DELAY);
                    }
                }

                auto sprt_entry = pool_make<ServiceEntry>(SdEntryType::OFFER_SERVICE, ref_service->get_info());
                ref_service->set_service_entry(sprt_entry);

                OptionVec service_entry_options;

                someIp::PoolPtr<Option> sprt_endpoint_option;
                if (false /* IpAddr is IPv4-only */)
                {
                    if (config::IPV6_ENABLED)
                    {
                        sprt_endpoint_option = pool_make<Ipv6Option>(ref_local_ip, ref_service->get_port());
                    }
                    else
                    {
                        SD_OFFER_LOGGER::error("IPV6 is not supported");
                        continue;
                    }
                }
                else if (true)
                {
                    if (config::IPV4_ENABLED)
                    {
                        sprt_endpoint_option = pool_make<Ipv4Option>(ref_local_ip, ref_service->get_port());
                    }
                    else
                    {
                        SD_OFFER_LOGGER::error("IPV4 is not supported");
                        continue;
                    }
                }
                service_entry_options.push_back(sprt_endpoint_option);

                validate_offer_service_options(service_entry_options, ref_service->get_options());
                ref_service->update_options(service_entry_options);

                ref_service->set_ready(true);
                m_ref_registry.add_offered(ref_service);
                prepared_services.push_back(ref_service);
            }

            if (prepared_services.empty())
            {
                SD_OFFER_LOGGER::error("No valid services to offer");
                return false;
            }
            SD_OFFER_LOGGER::debug_sd(
                "<<<<<<<<<< NUMBER OF SERVICES GOING TO BE OFFERED [%zu] >>>>>>>>>>>>>",
                prepared_services.size());

            lttng_ust_tracepoint(someip, start_offering_services);

            for (auto &ref_service : prepared_services)
            {
                _start_machine(ref_service);
            }

            return true;
        }

        bool SdOfferManager::stop_offer(SdServiceInfo service_info)
        {
            SdMessage message;
            auto [service_to_stop, found] = m_ref_registry.remove_offered_or_separate(service_info);
            if (!service_to_stop)
            {
                SD_OFFER_LOGGER::error("No registered service offered with this info");
                return false;
            }
            (void)found;
            service_to_stop->set_ready(false);

            _abort_machine(service_to_stop);

            service_info._ttl = SD_END_TTL;
            auto sprt_entry = pool_make<ServiceEntry>(SdEntryType::STOP_OFFER_SERVICE, service_info);
            message.add_entry(sprt_entry);
            message.add_entry_options(sprt_entry, SD_FIRST_OPTION_RUN, service_to_stop->get_options());
            return m_ref_transport.send_package(message, m_multicast_ip, config::SD_PORT, config::SD_PORT);
        }

        void SdOfferManager::stop_all()
        {
            SdMessage message;
            bool has_services_to_stop = false;

            auto [offered_services_copy, separate_services_copy] = m_ref_registry.take_offered_and_separate();

            for (auto &ref_service : offered_services_copy)
            {
                ref_service->set_ready(false);

                auto service_info = ref_service->get_info();
                service_info._ttl = SD_END_TTL;

                auto sprt_entry = pool_make<ServiceEntry>(SdEntryType::STOP_OFFER_SERVICE, service_info);
                message.add_entry(sprt_entry);
                message.add_entry_options(sprt_entry, SD_FIRST_OPTION_RUN, OptionVec{ref_service->get_options()[0]});

                _abort_machine(ref_service);

                has_services_to_stop = true;
            }

            for (auto &ref_service : separate_services_copy)
            {
                ref_service->set_ready(false);

                auto service_info = ref_service->get_info();
                service_info._ttl = SD_END_TTL;

                auto sprt_entry = pool_make<ServiceEntry>(SdEntryType::STOP_OFFER_SERVICE, service_info);
                message.add_entry(sprt_entry);
                message.add_entry_options(sprt_entry, SD_FIRST_OPTION_RUN, ref_service->get_options());

                _abort_machine(ref_service);

                has_services_to_stop = true;
                SD_OFFER_LOGGER::debug_sd(
                    "Added stop entry for separate offered service: %u",
                    service_info._service_id);
            }

            if (has_services_to_stop)
            {
                m_ref_transport.send_package(message, m_multicast_ip, config::SD_PORT, config::SD_PORT);
            }
        }

        void SdOfferManager::validate_offer_service_options(OptionVec &ref_valid_options,
                                                              const OptionVec &ref_options)
        {
            bool added[3] = {false, false, false};

            if (!ref_options.empty())
            {
                for (auto &ref_option : ref_options)
                {
                    switch (ref_option->get_type())
                    {
                    case SdOptionType::IPV4_MULTICAST:
                    case SdOptionType::IPV6_MULTICAST:
                        SD_OFFER_LOGGER::error("Offer Service can't have multicast options, going to be ignored");
                        break;

                    case SdOptionType::CONFIGURATION:
                        if (!added[0])
                        {
                            added[0] = true;
                            ref_valid_options.push_back(ref_option);
                            break;
                        }
                        [[fallthrough]];

                    case SdOptionType::LOAD_BALANCING:
                        if (!added[1])
                        {
                            added[1] = true;
                            ref_valid_options.push_back(ref_option);
                            break;
                        }
                        [[fallthrough]];

                    case SdOptionType::IPV4_ENDPOINT:
                    case SdOptionType::IPV6_ENDPOINT:
                        if (!added[2])
                        {
                            added[2] = true;
                            ref_valid_options.push_back(ref_option);
                            break;
                        }
                        [[fallthrough]];

                    default:
                        SD_OFFER_LOGGER::error("Offer Service has reached maximum number of options of type: ");
                        ref_option->print_type();
                        SD_OFFER_LOGGER::error(", going to be ignored");
                    }
                }
            }
            for (auto sprt_option : ref_options)
            {
                sprt_option->print_type();
            }
        }

    } // namespace sd
} // namespace someIp
