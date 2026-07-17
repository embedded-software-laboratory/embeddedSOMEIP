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

#ifndef EXAMPLES_COMMON_CONSTANTS_HPP
#define EXAMPLES_COMMON_CONSTANTS_HPP

#include <cstdint>
#include <cstddef>

namespace example {

inline constexpr uint16_t SERVICE_ID = 0x1001;
inline constexpr uint16_t INSTANCE_ID = 0x0001;
inline constexpr uint16_t MAJOR_VERSION = 1;
inline constexpr uint16_t MINOR_VERSION = 0;

inline constexpr uint16_t RPC_METHOD_ID = 0x0001;
inline constexpr uint16_t SHUTDOWN_METHOD_ID = 0x0002;
// fire and forget method used by the OWD latency test
inline constexpr uint16_t OWD_METHOD_ID = 0x0003;

inline constexpr uint16_t FIELD_GET_ID = 0x0010;
inline constexpr uint16_t FIELD_SET_ID = 0x0011;

inline constexpr uint16_t EVENT_ID = 0x8001;
inline constexpr uint16_t FIELD_EVENT_ID = 0x8002;
inline constexpr uint16_t EVENTGROUP_ID = 0x0001;

inline constexpr uint16_t SERVER_PORT = 8010;
inline constexpr uint16_t CLIENT_PORT = 5000;

// loopback on the POSIX host, TAP/VDE test addresses on STM32
#ifndef SOMEIP_PLATFORM_STM32
inline constexpr const char *SERVER_IP = "127.0.0.1";
inline constexpr const char *CLIENT_IP = "127.0.0.1";
#else
inline constexpr const char *SERVER_IP = "10.10.0.2";
inline constexpr const char *CLIENT_IP = "10.10.0.3";
#endif

inline constexpr size_t UDP_PAYLOAD_SIZE = 0;
inline constexpr size_t UDP_TP_PAYLOAD_SIZE = 2048;

} // namespace example

#endif // EXAMPLES_COMMON_CONSTANTS_HPP
