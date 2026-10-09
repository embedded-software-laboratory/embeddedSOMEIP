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

#ifndef SOMEIP_CONFIG_LINUX_HPP
#define SOMEIP_CONFIG_LINUX_HPP

#include <cstdint>
#include <cstddef>

#include "someIp/config/StackConfig.hpp" // all sizing/capacity knobs live here

namespace someIp
{
  namespace config
  {
    // should match settings in lwipopts.h
    const bool IPV6_ENABLED = false;
    const bool IPV4_ENABLED = true;

    const bool TCP_ENABLED = true;
    // enables per-packet LTTng tracepoints in the lwIP drivers (needs the session id from the wire)
    const bool IS_IN_EVALUATION_MODE = true;

    // sizing/capacity limits live in config/StackConfig.hpp

    // ---------------- SOME/IP ---------------- //

    const uint16_t CLIENT_ID = 0xABCD;
    const uint8_t PROTOCOL_VERSION = 0x01;
    // must match the offered service major version, RX path validates it
    const uint8_t INTERFACE_VERSION  = 0x01;
    const auto LOCAL_IP_ADDRESS = "0.0.0.0"; // unused on POSIX, the local IP is passed to init()
    const uint16_t LOCAL_PORT = 8012;

    // dynamic acquisition binds the first free port at or above this
    const uint16_t BASE_PORT = 40000;

    // bind a free unicast port unless the build asks for fixed ports
#if !defined(SOMEIP_FIXED_PORTS)
    constexpr bool DYNAMIC_UNICAST_PORT = true;
#else
    constexpr bool DYNAMIC_UNICAST_PORT = false;
#endif
    constexpr uint16_t DYNAMIC_PORT_RANGE = 64;

    // ---------------- SERVICE DISCOVERY ---------------- //
    const auto SD_MULTICAST_IP = "224.244.224.245";
    const uint16_t SD_PORT = 30490;
    const auto SD_MULTICAST_THRESHOLD = 1;
    constexpr int INITIAL_WAIT_MIN_MS = 10;
    constexpr int INITIAL_WAIT_MAX_MS = 100;
    constexpr int REPETITIONS_BASE_DELAY = 10;
    constexpr int MAX_REPETITION_DELAY = 600;
    constexpr int CYCLIC_OFFER_DELAY = 10;
    constexpr int REPETITIONS_MAX = 8;

  }
}

#endif // SOMEIP_CONFIG_LINUX_HPP
