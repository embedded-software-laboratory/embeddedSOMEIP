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

// server-side OFFER_SERVICE state machines, one SdOfferFsm per offered service
#ifndef SOMEIP_SD_OFFER_MANAGER_HPP_
#define SOMEIP_SD_OFFER_MANAGER_HPP_

#include <cstddef>
#include "someIp/utils/PoolPtr.hpp"
#include <memory>

#include "Def.hpp"
#include "SdService.hpp"
#include "SdServiceRegistry.hpp"
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
        enum class OfferState : std::size_t
        {
            NOT_READY,
            INITIAL_WAIT,
            REPETITION,
            MAIN,
            STOPPED,
            COUNT
        };

        enum class OfferEvent
        {
            START,
            TIMEOUT,
            REPETITIONS_DONE,
            ABORT
        };

        class SdOfferFsm
        {
        public:
            SdOfferFsm(someIp::PoolPtr<SdService> sprt_service,
                         SdTransport &ref_transport,
                         TimerScheduler &ref_timers);

            SdOfferFsm(const SdOfferFsm &) = delete;
            SdOfferFsm &operator=(const SdOfferFsm &) = delete;

            void start() { m_fsm.postEvent(OfferEvent::START); }
            void abort() { m_fsm.postEvent(OfferEvent::ABORT); }

            OfferState state() { return m_fsm.state(); }
            const someIp::PoolPtr<SdService> &service() const { return m_sprt_service; }

        private:
            using Machine = someIp::FSM<SdOfferFsm, OfferState, OfferEvent>;

            void prepare_initial_wait();
            void on_enter_repetition();
            void on_repetition_tick();
            void on_enter_main();
            void on_cyclic_tick();

            long long timeout_for(OfferState entered);
            void observe(OfferState from, OfferEvent event, OfferState to);

            void send_offer();

            static const Machine::StateDef m_kStates[static_cast<std::size_t>(OfferState::COUNT)];
            static const Machine::Transition m_kTransitions[];
            static const std::size_t m_kNumTransitions;

            someIp::PoolPtr<SdService> m_sprt_service;
            SdTransport &m_ref_transport;
            IpAddr m_multicast_ip;
            long long m_initial_delay_ms = 0;
            char m_name_buf[config::FSM_NAME_SIZE] = {};
            Machine m_fsm;
        };

        class SdOfferManager
        {
        public:
            SdOfferManager(SdServiceRegistry &ref_registry, SdTransport &ref_transport, TimerScheduler &ref_timers);
            ~SdOfferManager();

            SdOfferManager(const SdOfferManager &) = delete;
            SdOfferManager &operator=(const SdOfferManager &) = delete;

            // validate, prepare a service offer and start its per-service state machine
            bool start_offer(someIp::PoolPtr<SdService> sprt_service,
                             const IpAddr &ref_src_ip,
                             uint16_t src_port,
                             OptionVec options = {});

            // offer all registered local services, each via its own state machine
            bool offer_local_services(const IpAddr &ref_local_ip);

            // stop offering a single service and send STOP_OFFER_SERVICE
            bool stop_offer(SdServiceInfo service_info);

            // stop offering all services and send one batched STOP_OFFER_SERVICE
            void stop_all();

            // filter offer-service options (PRS option-run limits)
            static void validate_offer_service_options(OptionVec &ref_valid_options,
                                                       const OptionVec &ref_options);

        private:
            void _start_machine(someIp::PoolPtr<SdService> sprt_service);
            // post ABORT to this service's machine, caller must NOT hold _mtx
            void _abort_machine(const someIp::PoolPtr<SdService> &ref_service);

            StaticPool<SdOfferFsm, config::MAX_LOCAL_SERVICES> m_fsm_pool;
            StaticVector<SdOfferFsm *, config::MAX_LOCAL_SERVICES> m_machines;
            SdServiceRegistry &m_ref_registry;
            SdTransport &m_ref_transport;
            TimerScheduler &m_ref_timers;
            IpAddr m_multicast_ip;
            mutable os::Mutex m_mtx;
        };

    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_OFFER_MANAGER_HPP_
