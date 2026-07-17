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

// client-side FIND_SERVICE state machines, one SdFindFsm per requested service
#ifndef SOMEIP_SD_FIND_MANAGER_HPP_
#define SOMEIP_SD_FIND_MANAGER_HPP_

#include <cstddef>
#include "someIp/utils/PoolPtr.hpp"
#include <memory>

#include "Def.hpp"
#include "SdClientService.hpp"
#include "SdTransport.hpp"
#include "someIp/utils/Fsm.hpp"
#include "someIp/utils/TimerScheduler.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/utils/StaticPool.hpp"
#include "someIp/config/StackConfig.hpp"
#include "someIp/os/Os.hpp"

namespace someIp
{
    namespace sd
    {
        class ConfigurationOption;

        enum class FindState : std::size_t
        {
            NOT_REQUESTED,
            INITIAL_WAIT,
            REPETITION,
            MAIN,
            STOPPED,
            COUNT
        };

        enum class FindEvent
        {
            REQUEST,
            TIMEOUT,
            REPETITIONS_DONE,
            OFFER_RECEIVED,
            REMOTE_LOST
        };

        class SdFindFsm
        {
        public:
            SdFindFsm(someIp::PoolPtr<SdClientService> sprt_client,
                        SdTransport &ref_transport,
                        TimerScheduler &ref_timers);

            SdFindFsm(const SdFindFsm &) = delete;
            SdFindFsm &operator=(const SdFindFsm &) = delete;

            void request() { m_fsm.postEvent(FindEvent::REQUEST); }
            // REQUEST only if still in NOT_REQUESTED (atomic check)
            bool try_request() { return m_fsm.tryStep(FindState::NOT_REQUESTED, FindEvent::REQUEST); }
            void on_offer_received() { m_fsm.postEvent(FindEvent::OFFER_RECEIVED); }
            void on_remote_lost() { m_fsm.postEvent(FindEvent::REMOTE_LOST); }

            FindState state() { return m_fsm.state(); }
            const someIp::PoolPtr<SdClientService> &client() const { return m_sprt_client; }

        private:
            using Machine = someIp::FSM<SdFindFsm, FindState, FindEvent>;

            // transition actions
            void prepare_initial_wait();
            void on_enter_repetition();
            void on_repetition_tick();

            long long timeout_for(FindState entered);
            void observe(FindState from, FindEvent event, FindState to);

            void send_find();
            void repetition_step();

            static const Machine::StateDef m_kStates[static_cast<std::size_t>(FindState::COUNT)];
            static const Machine::Transition m_kTransitions[];
            static const std::size_t m_kNumTransitions;

            someIp::PoolPtr<SdClientService> m_sprt_client;
            SdTransport &m_ref_transport;
            IpAddr m_multicast_ip;
            long long m_initial_delay_ms = 0;
            char m_name_buf[config::FSM_NAME_SIZE] = {};
            Machine m_fsm;
        };

        class SdFindManager
        {
        public:
            SdFindManager(SdTransport &ref_transport, TimerScheduler &ref_timers);
            ~SdFindManager();

            SdFindManager(const SdFindManager &) = delete;
            SdFindManager &operator=(const SdFindManager &) = delete;

            // register a client service for discovery without sending FIND
            void register_client(someIp::PoolPtr<SdClientService> sprt_client);

            // request discovery, false if already in MAIN phase
            bool request(const SdServiceInfo &ref_service_info,
                         someIp::PoolPtr<ConfigurationOption> sprt_config_option = nullptr);

            // promote a matching find request to MAIN on offer received
            void on_offer_received(const SdServiceInfo &ref_service_info);

            // stop a matching find request on TTL expiry or STOP_OFFER
            void on_remote_lost(const SdServiceInfo &ref_service_info);

        private:
            // caller must hold _mtx
            SdFindFsm *_find_machine(const SdServiceInfo &ref_service_info) const;

            StaticPool<SdFindFsm, config::MAX_REMOTE_SERVICES> m_fsm_pool;
            StaticVector<SdFindFsm *, config::MAX_REMOTE_SERVICES> m_machines;
            SdTransport &m_ref_transport;
            TimerScheduler &m_ref_timers;
            mutable os::Mutex m_mtx;
        };

    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_FIND_MANAGER_HPP_
