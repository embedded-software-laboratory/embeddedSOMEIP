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

// tracks SD session IDs and reboot flags per remote endpoint and direction
#ifndef SOMEIP_SD_SESSION_MANAGER_HPP_
#define SOMEIP_SD_SESSION_MANAGER_HPP_

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "Def.hpp"
#ifdef SOMEIP_PLATFORM_STM32
#endif
#include "someIp/os/Os.hpp"
#include "someIp/utils/StaticMap.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp
{
    namespace sd
    {
        struct SdEndpointStatus
        {
            uint16_t _session_id;
            bool _reboot_flag;
            bool _counter_reset;
        };
    } // namespace sd
} // namespace someIp

namespace std
{
    template <>
    struct hash<someIp::sd::SdEndpointInfo>
    {
        std::size_t operator()(const someIp::sd::SdEndpointInfo &ref_info) const
        {
            uint32_t ip_raw = ref_info._ip.to_u32();
            std::size_t h1 = std::hash<uint32_t>{}(ip_raw);
            std::size_t h2 = std::hash<uint16_t>{}(ref_info._port);
            return h1 ^ (h2 << 1);
        }
    };
} // namespace std

namespace someIp
{
    namespace sd
    {
        class SdSessionManager
        {
        public:
            SdSessionManager();
            ~SdSessionManager();

            SdSessionManager(const SdSessionManager &) = delete;
            SdSessionManager &operator=(const SdSessionManager &) = delete;

            // advances session ID for outgoing messages, wraps at SD_MAX_SESSION_ID
            uint16_t next_sender_sid(const SdEndpointInfo &ref_endpoint, bool *ptr_reboot_flag = nullptr);

            // advances expected session ID for incoming messages
            uint16_t update_receivers_sid(const SdEndpointInfo &ref_endpoint);

            // updates stored reboot flag and reports whether a reboot was detected
            bool check_reboot(const SdEndpointInfo &ref_endpoint, bool new_reboot_flag);

        private:
            StaticMap<SdEndpointInfo, SdEndpointStatus, config::MAX_REMOTE_SERVICES> m_senders_sid;
            StaticMap<SdEndpointInfo, SdEndpointStatus, config::MAX_REMOTE_SERVICES> m_receivers_sid;
            mutable os::Mutex m_mtx;
        };

    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_SESSION_MANAGER_HPP_
