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

#include "TxSessionManager.hpp"

namespace someIp {
namespace tp {

// initial constructor for session
TxSessionManager::TxSession::TxSession(const Header& ref_header, uint32_t total_length)
    : header(ref_header), total_length(total_length), current_offset16(0), finished(false), active(true), id(ref_header._request_id.session_id) {}

// creates or replaces an entry for a session id
bool TxSessionManager::start_session(const Header& ref_header, const uint32_t& ref_total_length) {
    uint16_t session_id = ref_header._request_id.session_id;
    TxSession* ptr_slot = m_sessions.acquire(session_id); // existing (overwritten) or free
    if(!ptr_slot)
    {
        TX_SESSION_MANAGER_LOGGER::error("TxSession map is full");
        return false;
    }
    *ptr_slot = TxSession(ref_header, ref_total_length);
    return true;
}

bool TxSessionManager::has_active(uint16_t session_id) const {
    return m_sessions.has(session_id);
}

TxSessionManager::TxSession* TxSessionManager::get_session(uint16_t session_id) {
    return m_sessions.find(session_id);
}

void TxSessionManager::remove_session(uint16_t session_id) {
    m_sessions.release(session_id);
}

// increment the offset of current package when segmenter is called
void TxSessionManager::update_offset(uint16_t session_id, uint32_t bytes_sent) {
    if(auto* ptr_s = m_sessions.find(session_id); ptr_s)
        ptr_s->current_offset16 += bytes_sent/16;
    else
        TX_SESSION_MANAGER_LOGGER::debug("update_offset: unknown id");
}

void TxSessionManager::mark_finished(uint16_t session_id) {
    if(auto* ptr_s = m_sessions.find(session_id); ptr_s)
        ptr_s->finished = true;
    else
        TX_SESSION_MANAGER_LOGGER::debug("mark_finished: unknown id");
}

bool TxSessionManager::is_finished(uint16_t session_id) const {
    if(auto* ptr_s = m_sessions.find(session_id); ptr_s)
        return ptr_s->finished;
    TX_SESSION_MANAGER_LOGGER::debug("is_finished: unknown id");
    return false;
}

uint32_t TxSessionManager::current_offset(uint16_t session_id) const {
    if(auto* ptr_s = m_sessions.find(session_id); ptr_s)
        return ptr_s->current_offset16;
    TX_SESSION_MANAGER_LOGGER::debug("current_offset: unknown id");
    return 0;
}

uint32_t TxSessionManager::total_length(uint16_t session_id) const {
    if(auto* ptr_s = m_sessions.find(session_id); ptr_s)
        return ptr_s->total_length;
    TX_SESSION_MANAGER_LOGGER::debug("total_length: unknown id");
    return 0;
}

Header TxSessionManager::original_header(uint16_t session_id) const {
    if(auto* ptr_s = m_sessions.find(session_id); ptr_s)
        return ptr_s->header;
    TX_SESSION_MANAGER_LOGGER::debug("original_header: unknown id");
    return Header{};
}

void TxSessionManager::clear_all()
{
    m_sessions.clear_all();
}

} // namespace tp
} // namespace someIp
