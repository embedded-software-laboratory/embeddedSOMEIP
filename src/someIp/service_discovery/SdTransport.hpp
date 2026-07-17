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

// send interface for SD messages, breaks a dependency cycle
#ifndef SOMEIP_SD_TRANSPORT_HPP_
#define SOMEIP_SD_TRANSPORT_HPP_

#include <cstdint>

#include "someIp/net/IpAddress.hpp"
#include "SdMessage.hpp"

namespace someIp
{
    namespace sd
    {
        struct SdTransport
        {
            virtual bool send_package(SdMessage msg, IpAddr dest_ip, uint16_t dest_port, uint16_t src_port) = 0;
            virtual ~SdTransport() = default;
        };
    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_TRANSPORT_HPP_
