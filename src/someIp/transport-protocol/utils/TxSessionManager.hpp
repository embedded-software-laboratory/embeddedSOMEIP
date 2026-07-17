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

#ifndef SOMEIP_TP_TX_SESSION_MANAGER_HPP
#define SOMEIP_TP_TX_SESSION_MANAGER_HPP

#include <array>
#include <cstdint>
#include <cstring>

#include "someIp/structs/Header.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/pattern/Singleton.hpp"
#include "someIp/config/StackConfig.hpp"
#include "someIp/utils/SessionTable.hpp"

constexpr char TX_SESSION_MANAGER_TAG[] = "TX_S_M";
using TX_SESSION_MANAGER_LOGGER = BaseLogger<TX_SESSION_MANAGER_TAG>;


namespace someIp {
namespace tp {

static constexpr size_t MAX_TP_SESSIONS = someIp::config::TP_TX_SESSIONS;

    // tracks outgoing TP segmentation sessions, one active per session id
    class TxSessionManager
    {
    public:
        struct TxSession {
            Header header {};
            uint32_t total_length = {0}; // SDU total length [SWS_SomeIpTp_00001]
            uint32_t current_offset16 = {0}; // Offset in units of 16 bytes
            bool finished = {false};
            bool active = {false};
            uint16_t id = {0};

            // SessionTable hooks, keyed by bare session id
            bool matches(uint16_t k) const { return id == k; }
            void set_key(uint16_t k) { id = k; }

            TxSession() = default;
            TxSession(const Header& ref_header, uint32_t total_length);
        };

        TxSessionManager() = default;
        ~TxSessionManager() = default;

        // false if no free slots
        bool start_session(const Header& ref_header, const uint32_t& ref_total_length);
        bool has_active(uint16_t session_id) const;
        TxSession* get_session(uint16_t session_id);
        void remove_session(uint16_t session_id);

        void update_offset(uint16_t session_id, uint32_t bytes_sent);
        void mark_finished(uint16_t session_id);
        bool is_finished(uint16_t session_id) const;
        uint32_t current_offset(uint16_t session_id) const;
        uint32_t total_length(uint16_t session_id) const;
        Header original_header(uint16_t session_id) const;

        void clear_all();

    private:
        SessionTable<TxSession, MAX_TP_SESSIONS> m_sessions;
    };

} // namespace tp
} // namespace someIp

#endif // SOMEIP_TP_TX_SESSION_MANAGER_HPP
