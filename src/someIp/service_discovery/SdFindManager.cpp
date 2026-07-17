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
#include "SdFindManager.hpp"

#include <algorithm>
#include <random>

#include "ServiceEntry.hpp"
#include "SdMessage.hpp"
#include "someIp/config/config.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/os/Os.hpp"

#include "someip_tp.h"

constexpr char SD_FIND_MANAGER_TAG[] = "SD_FIND_MANAGER";
using SD_FIND_LOGGER = BaseLogger<SD_FIND_MANAGER_TAG>;

namespace someIp
{
    namespace sd
    {

        using S = FindState;
        using E = FindEvent;
        using A = SdFindFsm;

        const SdFindFsm::Machine::StateDef SdFindFsm::m_kStates[static_cast<std::size_t>(S::COUNT)] = {
            /* NOT_REQUESTED */ {0, E::TIMEOUT},
            /* INITIAL_WAIT  */ {Machine::DYNAMIC_TIMEOUT, E::TIMEOUT},
            /* REPETITION    */ {Machine::DYNAMIC_TIMEOUT, E::TIMEOUT},
            /* MAIN          */ {0, E::TIMEOUT},
            /* STOPPED       */ {0, E::TIMEOUT},
        };

        const SdFindFsm::Machine::Transition SdFindFsm::m_kTransitions[] = {
            {S::NOT_REQUESTED, E::REQUEST, S::INITIAL_WAIT, &A::prepare_initial_wait},
            {S::INITIAL_WAIT, E::TIMEOUT, S::REPETITION, &A::on_enter_repetition},
            {S::REPETITION, E::TIMEOUT, S::REPETITION, &A::on_repetition_tick},
            {S::REPETITION, E::REPETITIONS_DONE, S::STOPPED, nullptr},
            // an offer promotes the request to MAIN from any phase
            {S::NOT_REQUESTED, E::OFFER_RECEIVED, S::MAIN, nullptr},
            {S::INITIAL_WAIT, E::OFFER_RECEIVED, S::MAIN, nullptr},
            {S::REPETITION, E::OFFER_RECEIVED, S::MAIN, nullptr},
            {S::MAIN, E::OFFER_RECEIVED, S::MAIN, nullptr},
            {S::STOPPED, E::OFFER_RECEIVED, S::MAIN, nullptr},
            {S::NOT_REQUESTED, E::REMOTE_LOST, S::STOPPED, nullptr},
            {S::INITIAL_WAIT, E::REMOTE_LOST, S::STOPPED, nullptr},
            {S::REPETITION, E::REMOTE_LOST, S::STOPPED, nullptr},
            {S::MAIN, E::REMOTE_LOST, S::STOPPED, nullptr},
        };

        const std::size_t SdFindFsm::m_kNumTransitions = sizeof(SdFindFsm::m_kTransitions) / sizeof(SdFindFsm::m_kTransitions[0]);

        SdFindFsm::SdFindFsm(someIp::PoolPtr<SdClientService> sprt_client,
                                 SdTransport &ref_transport,
                                 TimerScheduler &ref_timers)
            : m_sprt_client(std::move(sprt_client)),
              m_ref_transport(ref_transport),
              m_fsm(S::NOT_REQUESTED, m_kStates, static_cast<std::size_t>(S::COUNT),
                   m_kTransitions, m_kNumTransitions, this, ref_timers,
                   fsm_name(m_name_buf, sizeof(m_name_buf), "FIND_SERVICE_", m_sprt_client->get_info()._service_id))
        {
            IpAddr::from_string(config::SD_MULTICAST_IP, m_multicast_ip);
            m_fsm.setTimeoutProvider(&A::timeout_for);
            m_fsm.setObserver(&A::observe);
            m_fsm.start();
        }

        void SdFindFsm::prepare_initial_wait()
        {
            // [PRS_SOMEIPSD_00201] random initial wait avoids congestion
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(config::INITIAL_WAIT_MIN_MS, config::INITIAL_WAIT_MAX_MS);
            m_initial_delay_ms = dis(gen);

            lttng_ust_tracepoint(someip, find_service_initial_phase_started,
                                 m_sprt_client->get_info()._service_id,
                                 m_sprt_client->get_info()._instance_id,
                                 m_sprt_client->get_info()._major_version,
                                 m_sprt_client->get_info()._minor_version,
                                 m_initial_delay_ms);
        }

        void SdFindFsm::on_enter_repetition()
        {
            lttng_ust_tracepoint(someip, find_service_repetition_phase_started,
                                 m_sprt_client->get_info()._service_id,
                                 m_sprt_client->get_info()._instance_id,
                                 m_sprt_client->get_info()._major_version,
                                 m_sprt_client->get_info()._minor_version);
            m_sprt_client->_current_delay = Duration(config::REPETITIONS_BASE_DELAY);
            m_sprt_client->_total_delay = Duration(0);
            m_sprt_client->_repetition = 0;

            repetition_step();
        }

        void SdFindFsm::on_repetition_tick()
        {
            repetition_step();
        }

        void SdFindFsm::repetition_step()
        {
            send_find();

            if (++m_sprt_client->_repetition >= config::REPETITIONS_MAX ||
                m_sprt_client->_total_delay.count() >= config::MAX_REPETITION_DELAY)
            {
                SD_FIND_LOGGER::debug_sd(
                    "##################################################################\n"
                    "REPETITION PHASE FINISHED WITHOUT FINDING THE SERVICE; NOW STOPPED\n"
                    "##################################################################");
                m_fsm.postEvent(E::REPETITIONS_DONE);
                return;
            }

            // [PRS_SOMEIPSD_00406] delay doubles after each repetition
            m_sprt_client->_total_delay += m_sprt_client->_current_delay;
            m_sprt_client->_current_delay *= 2;
        }

        void SdFindFsm::send_find()
        {
            const auto &ref_info = m_sprt_client->get_info();

            SdMessage msg;
            auto sprt_entry = pool_make<ServiceEntry>(SdEntryType::FIND_SERVICE, ref_info);
            msg.add_entry(sprt_entry);

            lttng_ust_tracepoint(someip,
                                 find_service_sent,
                                 ref_info._service_id,
                                 ref_info._instance_id,
                                 ref_info._major_version,
                                 ref_info._minor_version);

            m_ref_transport.send_package(msg, m_multicast_ip, config::SD_PORT, config::SD_PORT);
        }

        long long SdFindFsm::timeout_for(FindState entered)
        {
            switch (entered)
            {
            case S::INITIAL_WAIT:
                return m_initial_delay_ms;
            case S::REPETITION:
                // delay already doubled by the previous repetition step
                return static_cast<long long>(m_sprt_client->_current_delay.count());
            default:
                return 0;
            }
        }

        void SdFindFsm::observe(FindState from, FindEvent event, FindState to)
        {
            (void)from;
            switch (to)
            {
            case S::INITIAL_WAIT:
                m_sprt_client->set_phase(SdClientPhase::INITIAL_WAIT);
                break;
            case S::REPETITION:
                m_sprt_client->set_phase(SdClientPhase::REPETITION);
                break;
            case S::MAIN:
                m_sprt_client->set_phase(SdClientPhase::MAIN);
                lttng_ust_tracepoint(someip, find_service_main_phase_started,
                                     m_sprt_client->get_info()._service_id,
                                     m_sprt_client->get_info()._instance_id,
                                     m_sprt_client->get_info()._major_version,
                                     m_sprt_client->get_info()._minor_version);
                break;
            case S::STOPPED:
                m_sprt_client->set_phase(SdClientPhase::STOPPED);
                if (event == E::REPETITIONS_DONE)
                {
                    lttng_ust_tracepoint(someip,
                                         find_service_stop_phase,
                                         m_sprt_client->get_info()._service_id,
                                         m_sprt_client->get_info()._instance_id,
                                         m_sprt_client->get_info()._major_version,
                                         m_sprt_client->get_info()._minor_version);
                }
                break;
            default:
                break;
            }
        }

        SdFindManager::SdFindManager(SdTransport &ref_transport, TimerScheduler &ref_timers)
            : m_ref_transport(ref_transport), m_ref_timers(ref_timers)
        {
        }

        SdFindManager::~SdFindManager()
        {
        }

        SdFindFsm *SdFindManager::_find_machine(const SdServiceInfo &ref_service_info) const
        {
            auto ptr_it = std::find_if(m_machines.begin(), m_machines.end(),
                                   [&ref_service_info](SdFindFsm *ptr_machine)
                                   { return ptr_machine->client()->get_info() == ref_service_info; });
            return ptr_it != m_machines.end() ? *ptr_it : nullptr;
        }

        void SdFindManager::register_client(someIp::PoolPtr<SdClientService> sprt_client)
        {
            os::Guard lock(m_mtx);
            if (_find_machine(sprt_client->get_info()) != nullptr)
            {
                return;
            }
            m_machines.push_back(m_fsm_pool.acquire(std::move(sprt_client), m_ref_transport, m_ref_timers));
        }

        bool SdFindManager::request(const SdServiceInfo &ref_service_info,
                                      someIp::PoolPtr<ConfigurationOption> sprt_config_option)
        {
            (void)sprt_config_option; // accepted for API compatibility, not evaluated
            SdFindFsm *ptr_machine = nullptr;
            {
                os::Guard lock(m_mtx);
                ptr_machine = _find_machine(ref_service_info);
                if (ptr_machine == nullptr)
                {
                    auto sprt_client_service = pool_make<SdClientService>(ref_service_info);
                    m_machines.push_back(m_fsm_pool.acquire(std::move(sprt_client_service), m_ref_transport, m_ref_timers));
                    ptr_machine = m_machines.back();
                }
            }
            // post events outside the manager lock
            if (ptr_machine->state() == FindState::MAIN)
            {
                SD_FIND_LOGGER::error("Service already in MAIN phase");
                return false;
            }
            ptr_machine->try_request();
            return true;
        }

        void SdFindManager::on_offer_received(const SdServiceInfo &ref_service_info)
        {
            SdFindFsm *ptr_machine = nullptr;
            {
                os::Guard lock(m_mtx);
                ptr_machine = _find_machine(ref_service_info);
            }
            if (ptr_machine != nullptr)
            {
                SD_FIND_LOGGER::debug_sd(
                    "Service found in requested services, setting phase to MAIN");
                ptr_machine->on_offer_received();
            }
            else
            {
                SD_FIND_LOGGER::debug_sd(
                    "***** Service not found in requested services, not setting phase");
            }
        }

        void SdFindManager::on_remote_lost(const SdServiceInfo &ref_service_info)
        {
            SdFindFsm *ptr_machine = nullptr;
            {
                os::Guard lock(m_mtx);
                ptr_machine = _find_machine(ref_service_info);
            }
            if (ptr_machine != nullptr)
            {
                ptr_machine->on_remote_lost();
            }
        }

    } // namespace sd
} // namespace someIp
