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

#include <cstdio>
#include "Reassembler.hpp"
#include "someIp/ESomeIp.hpp"
#include "someIp/utils/WireBounds.hpp"

namespace someIp {
namespace tp {

// caller holds m_mtx
void Reassembler::arm_timeout(RxSession* ptr_session) {
    cancel_timeout(ptr_session);
    if (!m_ptr_scheduler) return; // no scheduler bound -> cleanup is best-effort
    const uint32_t gen = ptr_session->timeout_gen;
    RxSession* ptr_s = ptr_session;
    ptr_session->timeout_timer = m_ptr_scheduler->add_timer(
        "tp_rx", static_cast<int>(TP_Config.RxTimeoutTime),
        [this, ptr_s, gen]() {
            os::Guard g(m_mtx);
            // ignore a late callback whose timer was cancelled/re-armed
            if (ptr_s->active && ptr_s->timeout_gen == gen) {
                TP_REASSEMBLER_LOGGER::error("RX session %u timed out", ptr_s->id);
                ptr_s->active = false;
                ptr_s->interrupted = true;
                ptr_s->buffer.clear();
            }
        },
        false);
}

// gen bump no-ops a late callback (report M-16/C-8)
void Reassembler::cancel_timeout(RxSession* ptr_session) {
    ptr_session->timeout_gen++;
    if (m_ptr_scheduler && ptr_session->timeout_timer) {
        m_ptr_scheduler->cancel_timer_nowait(ptr_session->timeout_timer);
        ptr_session->timeout_timer.reset();
    }
}

void Reassembler::clear_all() {
    for (auto& ref_session : m_sessions) {
        ref_session.active = false;
        ref_session.interrupted = false;
        ref_session.lastSeen = false;
        ref_session.clear_coverage();
        ref_session.totalSize = 0;
        ref_session.buffer.clear();
        ref_session.key = {};
        ref_session.sourceIp = {};
        ref_session.sourcePort = 0;
    }
}

Reassembler::Reassembler() {
    clear_all();
}

bool Reassembler::pushFragment(PackageRx&& ref_frag, void* ptr_arg) {
    os::Guard guard(m_mtx); // serialize the receiver threads + the timeout callback
    Header header;
    if (ref_frag.peek_header(&header) != ReturnCode::E_OK)
        return false;

    // key is source endpoint plus message identity (report C-3)
    RxSessionKey key{};
    key.src_ip = ref_frag.get_source_ip();
    key.src_port = ref_frag.get_source_port();
    key.client_id = header._request_id.client_id;
    key.session_id = header._request_id.session_id;
    key.service_id = header._message_id._service_id;
    key.method_id = header._message_id._method_id;

    // Bit 5 of Message Type is the TP-Flag (0x20)
    if ((static_cast<uint8_t>(header._message_type) & 0x20u) == 0u) {
        // drop any stale session for this key
        if (auto* ptr_old = find(key); ptr_old && ptr_old->active) {
            ptr_old->active = false;
            ptr_old->buffer.clear();
        }
        ref_frag.strip_tp_header();
        if (auto *ptr_api = static_cast<someIp::ESomeIp *>(ptr_arg)) { ptr_api->add_to_queue(std::move(ref_frag)); }
        return true;
    }

    uint8_t tp_flag = static_cast<uint8_t>(header._message_type);
    header._message_type = static_cast<MessageType>(tp_flag & ~0x20);
    header._length = 0;

    // extract the 4-byte TP header (offset, reserved, MSF)
    auto tpCopy = ref_frag.payload.slice_copy(4, sizeof(header));
    if (!tpCopy) return false;

    uint32_t raw;
    std::memcpy(&raw, tpCopy.data(), sizeof(raw));
    raw = ensure_host_order(raw);

    TpOnlyHeader tph;
    tph.full_tponlyheader = raw;
    uint32_t offset = tph.get_offset() * 16;
    bool msf = tph.get_msf();
    uint8_t reserved = tph.get_reserved();
    uint16_t sid = header._request_id.session_id;

    if (reserved != 0) return false;

    // single self-contained TP PDU (offset 0, no more segments)
    if (offset == 0 && !msf) {
        if (auto* ptr_old = find(key); ptr_old && ptr_old->active) {
            ptr_old->active = false;
            ptr_old->buffer.clear();
        }
        ref_frag.strip_tp_header();
        if (auto *ptr_api = static_cast<someIp::ESomeIp *>(ptr_arg)) { ptr_api->add_to_queue(std::move(ref_frag)); }
        return true;
    }

    RxSession* ptr_session = find(key);
    if (ptr_session && ptr_session->interrupted) return false;

    if (!ptr_session) {
        // fragments are accepted in any order
        ptr_session = m_sessions.acquire_free(key);
        if (!ptr_session) return false;
        ptr_session->active = true;
        ptr_session->interrupted = false;
        ptr_session->lastSeen = false;
        ptr_session->header = header; // TP flag already cleared above
        ptr_session->clear_coverage();
        ptr_session->totalSize = 0;
        ptr_session->buffer.clear();
        ptr_session->sourceIp = ref_frag.get_source_ip();
        ptr_session->sourcePort = ref_frag.get_source_port();
        arm_timeout(ptr_session);
    } else {
        // non-key mismatch drops the fragment, not the session (report C-3)
        auto& ref_orig = ptr_session->header;
        if (header._protocol_version != ref_orig._protocol_version ||
            header._interface_version != ref_orig._interface_version ||
            header._message_type != ref_orig._message_type ||
            header._return_code != ref_orig._return_code) {
            return false;
        }
    }

    ref_frag.delete_header();
    const uint16_t payloadLen = ref_frag.payload.size();

    // intermediate (more-segments) fragments must be multiples of 16 bytes
    if (msf && (payloadLen % 16 != 0)) {
        ptr_session->active = false;
        ptr_session->interrupted = true;
        ptr_session->buffer.clear();
        return false;
    }

    // bound in 64-bit to avoid a 32-bit wrap (report C-1)
    if (!wire::within_bound(offset, payloadLen, config::MAX_TP_RX_MESSAGE)) {
        TP_REASSEMBLER_LOGGER::error("TP offset/length out of bounds; dropping session");
        ptr_session->active = false;
        ptr_session->interrupted = true;
        ptr_session->buffer.clear();
        return false;
    }

    // store the fragment at its byte offset
    if (ptr_session->buffer.size() < static_cast<size_t>(offset) + payloadLen) {
        if (!ptr_session->buffer.resize(static_cast<size_t>(offset) + payloadLen)) {
            TP_REASSEMBLER_LOGGER::error("TP message too large or reassembly pool exhausted; dropping session");
            ptr_session->active = false;
            ptr_session->interrupted = true;
            ptr_session->buffer.clear();
            return false;
        }
    }
    ref_frag.payload.copy_out(ptr_session->buffer.data() + offset, payloadLen, 0);
    // track coverage so overlaps cannot fake completion (report C-4)
    ptr_session->mark_covered(offset, payloadLen);

    arm_timeout(ptr_session);

    // final segment (MSF=0) fixes the total message size
    if (!msf) {
        ptr_session->lastSeen = true;
        ptr_session->totalSize = static_cast<uint32_t>(offset) + payloadLen;
    }

    // needs the final segment and no holes (report C-4)
    if (!ptr_session->lastSeen || !ptr_session->fully_covered(ptr_session->totalSize)) {
        return false;
    }

    Header finalhdr = ptr_session->header;
    finalhdr._length = 8 + ptr_session->totalSize; // length excludes first 8 header bytes
    finalhdr._message_type = static_cast<MessageType>(static_cast<uint8_t>(finalhdr._message_type) & ~0x20);

    Header netHeader = finalhdr;
    netHeader._message_id._service_id = ensure_network_order(netHeader._message_id._service_id);
    netHeader._message_id._method_id = ensure_network_order(netHeader._message_id._method_id);
    netHeader._request_id.client_id = ensure_network_order(netHeader._request_id.client_id);
    netHeader._request_id.session_id = ensure_network_order(netHeader._request_id.session_id);
    netHeader._length = ensure_network_order(netHeader._length);

    PacketBuffer body(ptr_session->totalSize + sizeof(Header));
    std::memcpy(body.data(), &netHeader, sizeof(Header));
    std::memcpy(body.data() + sizeof(Header), ptr_session->buffer.data(), ptr_session->totalSize);

    PackageRx result(std::move(body), ptr_session->sourceIp, ptr_session->sourcePort);

    result.header = finalhdr;

    cancel_timeout(ptr_session);
    ptr_session->active = false;
    ptr_session->interrupted = false;
    ptr_session->lastSeen = false;
    ptr_session->clear_coverage();
    ptr_session->totalSize = 0;
    ptr_session->buffer.clear();
    ptr_session->key = {};
    ptr_session->sourceIp = {};
    ptr_session->sourcePort = 0;

    if (auto *ptr_api = static_cast<someIp::ESomeIp *>(ptr_arg)) { ptr_api->add_to_queue(std::move(result)); }
    return true;
}

} // namespace tp
} // namespace someIp
