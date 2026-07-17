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

#include "SdSessionManager.hpp"

#include "someIp/logging/BaseLogger.hpp"
#include "someIp/os/Os.hpp"

constexpr char SD_SESSION_MANAGER_TAG[] = "SD_SESSION_MANAGER";
using SD_SESSION_LOGGER = BaseLogger<SD_SESSION_MANAGER_TAG>;

namespace someIp
{
    namespace sd
    {

        SdSessionManager::SdSessionManager()
        {
        }

        SdSessionManager::~SdSessionManager()
        {
        }

        uint16_t SdSessionManager::next_sender_sid(const SdEndpointInfo &ref_endpoint, bool *ptr_reboot_flag)
        {
            os::Guard lock(m_mtx);
            auto ptr_it = m_senders_sid.find(ref_endpoint);
            if (ptr_it == m_senders_sid.end())
            {
                // add endpoint if not exist
                SdEndpointStatus &ref_status = m_senders_sid[ref_endpoint];
                ref_status._session_id = 1;
                ref_status._reboot_flag = true;
                SD_SESSION_LOGGER::debug_sd(
                    "[UPDATE_SENDER_SESSION]: added sessionID[%s:%u] = %u",
                    ref_endpoint._ip.c_str(),
                    ref_endpoint._port,
                    ref_status._session_id);

                if (ptr_reboot_flag != nullptr)
                    *ptr_reboot_flag = ref_status._reboot_flag;
                return 1;
            }
            else
            {
                if (ptr_it->second._session_id == SD_MAX_SESSION_ID)
                {
                    SD_SESSION_LOGGER::debug_sd("<--------------------################------------------------->");
                    SD_SESSION_LOGGER::debug_sd(
                        "[UPDATE_SENDER_SESSION]: SESSION_ID RESETTED FOR ENDPOINT: [%s:%u]",
                        ref_endpoint._ip.c_str(),
                        ref_endpoint._port);
                    SD_SESSION_LOGGER::debug_sd("<--------------------################------------------------->");

                    ptr_it->second._session_id = 1;
                    ptr_it->second._counter_reset = true;
                    ptr_it->second._reboot_flag = false;

                    if (ptr_reboot_flag != nullptr)
                        *ptr_reboot_flag = ptr_it->second._reboot_flag;
                    return 1;
                }
                SD_SESSION_LOGGER::debug_sd(
                    "[UPDATE_SENDER_SESSION]: sessionID[%s:%u] = %u",
                    ptr_it->first._ip.c_str(),
                    ptr_it->first._port,
                    ptr_it->second._session_id + 1);

                if (ptr_reboot_flag != nullptr)
                    *ptr_reboot_flag = ptr_it->second._reboot_flag;
                // increment session ID if exist
                return ++(ptr_it->second._session_id);
            }
        }

        uint16_t SdSessionManager::update_receivers_sid(const SdEndpointInfo &ref_endpoint)
        {
            os::Guard lock(m_mtx);
            auto ptr_it = m_receivers_sid.find(ref_endpoint);
            if (ptr_it == m_receivers_sid.end())
            {
                SdEndpointStatus &ref_status = m_receivers_sid[ref_endpoint];
                ref_status._session_id = 1;
                ref_status._reboot_flag = 1;
                SD_SESSION_LOGGER::debug_sd("[UPDATE_SESSION]: added sessionID[%s:%u] = %u",
                                            ref_endpoint._ip.c_str(),
                                            ref_endpoint._port,
                                            ref_status._session_id);
                return 1;
            }
            else
            {
                if (ptr_it->second._session_id == SD_MAX_SESSION_ID)
                {
                    SD_SESSION_LOGGER::debug_sd("<--------------------################------------------------->");
                    SD_SESSION_LOGGER::debug_sd("[UPDATE_SESSION]: SESSION_ID RESETTED FOR ENDPOINT: [%s:%u]",
                                                ref_endpoint._ip.c_str(),
                                                ref_endpoint._port);
                    SD_SESSION_LOGGER::debug_sd("<--------------------################------------------------->");
                    ptr_it->second._session_id = 1;
                    ptr_it->second._counter_reset = true;
                    return 1;
                }
                SD_SESSION_LOGGER::debug_sd(
                    "[UPDATE_SESSION]: sessionID[%s:%u] = %u",
                    ptr_it->first._ip.c_str(),
                    ptr_it->first._port,
                    ptr_it->second._session_id + 1);

                return ++(ptr_it->second._session_id);
            }
        }

        bool SdSessionManager::check_reboot(const SdEndpointInfo &ref_endpoint, bool new_reboot_flag)
        {
            os::Guard lock(m_mtx);
            SdEndpointStatus &ref_status = m_receivers_sid[ref_endpoint];
            SD_SESSION_LOGGER::debug_sd("NEW REBOOT: %u", new_reboot_flag);
            SD_SESSION_LOGGER::debug_sd("OLD REBOOT: %u", ref_status._reboot_flag);

            bool reboot_detected = ref_status._reboot_flag == false && new_reboot_flag == true;
            ref_status._reboot_flag = new_reboot_flag;

            if (ref_status._counter_reset)
            {
                reboot_detected = true;
                ref_status._counter_reset = false;
            }

            return reboot_detected;
        }

    } // namespace sd
} // namespace someIp
