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

#ifndef SOMEIP_TP_REASSEMBLER_HPP
#define SOMEIP_TP_REASSEMBLER_HPP

#include <memory>
#include "someIp/utils/PoolPtr.hpp"
#include <array>
#include <cstdint>
#include <cstring>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/Header.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/TPHeader.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/transport-protocol/config/Tp_Config.hpp"
#include "someIp/pattern/Singleton.hpp"
#include "someIp/utils/SessionTable.hpp"
#include "someIp/utils/StaticPool.hpp"
#include "someIp/config/StackConfig.hpp"
#include "someIp/utils/TimerScheduler.hpp"
#include "someIp/os/Os.hpp"

constexpr char TP_REASSEMBLER_TAG[] = "TP_REASM";
using TP_REASSEMBLER_LOGGER = BaseLogger<TP_REASSEMBLER_TAG>;

static constexpr size_t MAX_TP_RX_SESSIONS = someIp::config::TP_RX_SESSIONS;

namespace someIp
{
namespace tp
{
    // fixed block from a static pool, no heap
    struct TpReassemblyBlock { uint8_t data[config::MAX_TP_RX_MESSAGE]; };

    inline StaticPool<TpReassemblyBlock, config::TP_REASSEMBLY_POOL_BLOCKS> &tp_reassembly_pool() {
        static StaticPool<TpReassemblyBlock, config::TP_REASSEMBLY_POOL_BLOCKS> pool;
        return pool;
    }

    // holds at most one pooled block
    class ReassemblyBuffer {
    public:
        ReassemblyBuffer() = default;
        ReassemblyBuffer(const ReassemblyBuffer &) = delete;
        ReassemblyBuffer &operator=(const ReassemblyBuffer &) = delete;
        ReassemblyBuffer(ReassemblyBuffer &&ref_o) noexcept : m_ptr_block(ref_o.m_ptr_block), m_size(ref_o.m_size) {
            ref_o.m_ptr_block = nullptr; ref_o.m_size = 0;
        }
        ReassemblyBuffer &operator=(ReassemblyBuffer &&ref_o) noexcept {
            if (this != &ref_o) { clear(); m_ptr_block = ref_o.m_ptr_block; m_size = ref_o.m_size; ref_o.m_ptr_block = nullptr; ref_o.m_size = 0; }
            return *this;
        }
        ~ReassemblyBuffer() { clear(); }

        void clear() {
            if (m_ptr_block) { tp_reassembly_pool().release(m_ptr_block); m_ptr_block = nullptr; }
            m_size = 0;
        }
        size_t size() const { return m_size; }
        // grows the high-water mark only, false if too big
        bool resize(size_t n) {
            if (n > config::MAX_TP_RX_MESSAGE) return false;
            if (!m_ptr_block) { m_ptr_block = tp_reassembly_pool().acquire(); if (!m_ptr_block) return false; }
            if (n > m_size) m_size = n;
            return true;
        }
        uint8_t *data() { return m_ptr_block ? m_ptr_block->data : nullptr; }
        const uint8_t *data() const { return m_ptr_block ? m_ptr_block->data : nullptr; }

    private:
        TpReassemblyBlock *m_ptr_block = nullptr;
        size_t m_size = 0;
    };

    // source endpoint plus message identity (report C-3)
    struct RxSessionKey {
        IpAddr src_ip{};
        uint16_t src_port = 0;
        uint16_t client_id = 0;
        uint16_t session_id = 0;
        uint16_t service_id = 0;
        uint16_t method_id = 0;

        bool operator==(const RxSessionKey &ref_o) const {
            return src_port == ref_o.src_port && client_id == ref_o.client_id &&
                   session_id == ref_o.session_id && service_id == ref_o.service_id &&
                   method_id == ref_o.method_id && src_ip == ref_o.src_ip;
        }
    };

    struct RxSession
    {
        // 16-byte coverage blocks for a full MAX_TP_RX_MESSAGE buffer
        static constexpr size_t COVERAGE_BYTES = (config::MAX_TP_RX_MESSAGE / 16 + 7) / 8;

        bool active = false;
        bool interrupted = false;
        bool lastSeen = false;          // final (MSF=0) segment has arrived
        uint16_t id = 0;
        RxSessionKey key{};             // composite identity (report C-3)
        Header header;
        uint32_t totalSize = 0;         // known once the final segment arrives
        ReassemblyBuffer buffer;        // pooled buffer indexed by offset
        std::array<uint8_t, COVERAGE_BYTES> coverage{}; // received 16-byte blocks (report C-4)
        IpAddr sourceIp;
        uint16_t sourcePort;
        someIp::PoolPtr<TimerEvent> timeout_timer; // RX-stall timeout
        uint32_t timeout_gen = 0;       // bumped on (re)arm/cancel to no-op late callbacks

        // SessionTable hooks
        bool matches(const RxSessionKey &ref_k) const { return key == ref_k; }
        void set_key(const RxSessionKey &ref_k) { key = ref_k; id = ref_k.session_id; }

        // marks 16-byte blocks in [off, off+len), idempotent (report C-4)
        void mark_covered(uint32_t off, uint32_t len) {
            uint32_t b0 = off / 16;
            uint32_t b1 = (off + len + 15) / 16;
            for (uint32_t b = b0; b < b1; ++b) coverage[b >> 3] |= static_cast<uint8_t>(1u << (b & 7));
        }
        bool fully_covered(uint32_t total) const {
            uint32_t blocks = (total + 15) / 16;
            for (uint32_t b = 0; b < blocks; ++b)
                if (!(coverage[b >> 3] & (1u << (b & 7)))) return false;
            return true;
        }
        void clear_coverage() { coverage.fill(0); }

        RxSession() = default;
    };

    class Reassembler : public Singleton<Reassembler>
    {
    public:
        static Reassembler* get_instance() {
            return Singleton<Reassembler>::get_instance();
        }

        Reassembler();
        bool pushFragment(PackageRx&& ref_frag, void* ptr_arg);
        void clear_all();

        // RX timeouts run here so stalls clear without an lwIP thread
        void set_scheduler(TimerScheduler* ptr_scheduler) { m_ptr_scheduler = ptr_scheduler; }

        // in-progress reassembly sessions, for tests
        size_t active_session_count() {
            os::Guard g(m_mtx);
            return m_sessions.active_count();
        }

    private:
        SessionTable<RxSession, MAX_TP_RX_SESSIONS, RxSessionKey> m_sessions;
        TimerScheduler* m_ptr_scheduler = nullptr;
        os::Mutex m_mtx; // guards m_sessions across receivers and the timeout callback

        RxSession* find(const RxSessionKey& ref_k) { return m_sessions.find(ref_k); }
        void arm_timeout(RxSession* ptr_session);
        void cancel_timeout(RxSession* ptr_session);

        friend class Singleton<Reassembler>;
    };

} // namespace tp
} // namespace someIp

#endif // SOMEIP_TP_REASSEMBLER_HPP
