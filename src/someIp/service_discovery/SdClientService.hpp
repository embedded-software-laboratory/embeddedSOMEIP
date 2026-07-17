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

#ifndef SOMEIP_SD_CLIENT_SERVICE_HPP_
#define SOMEIP_SD_CLIENT_SERVICE_HPP_

#include "Def.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include "someIp/utils/timer.hpp"
#include "someIp/config/config.hpp"

namespace someIp
{
    namespace sd
    {

        enum class SdClientPhase
        {
            NOT_REQUESTED,
            REQUESTED_BUT_NOT_READY,
            INITIAL_WAIT,
            REPETITION,
            MAIN,
            STOPPED
        };

        class SdClientService
        {
        public:
            SdClientService(SdServiceInfo info)
                : m_info(info), m_phase(SdClientPhase::NOT_REQUESTED), _repetition(0) {}

            void set_phase(SdClientPhase phase) { m_phase = phase; }
            SdClientPhase get_phase() const { return m_phase; }

            const SdServiceInfo &get_info() const { return m_info; }

            int _repetition = 0;
            Duration _current_delay = Duration(0);
            Duration _total_delay = Duration(0);
            someIp::PoolPtr<TimerEvent> _ttl_timer;

        private:
            SdServiceInfo m_info;
            SdClientPhase m_phase;
        };

    } // sd
} // someIp
#endif // SOMEIP_SD_CLIENT_SERVICE_HPP
