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

#ifndef SOMEIP_TP_CONFIG_HPP
#define SOMEIP_TP_CONFIG_HPP

#include <cstdint>

#include "someIp/config/StackConfig.hpp"

namespace someIp
{
    namespace tp
    {
        struct ConfigType {
            uint32_t TxNPduLength   = config::TP_PDU_LENGTH;         // [SWS_SomeIpTp_00004]
            uint16_t TxBurstSize    = config::TP_BURST_SIZE;         // segments per burst
            uint16_t SeperationTime = config::TP_SEPARATION_TIME_MS; // [ms] between bursts
            uint32_t RxTimeoutTime  = config::TP_RX_TIMEOUT_MS;      // [ECUC_SomeIpTp_00023]
        };
        
        // global default, runtime variation is per-instance
        inline constexpr ConfigType TP_Config{};
        
    } // namespace tp
} // namespace someip


#endif