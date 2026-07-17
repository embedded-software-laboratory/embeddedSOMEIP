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

#ifndef SOMEIP_TCP_CONNECTION_HPP
#define SOMEIP_TCP_CONNECTION_HPP

#include <array>
#include <cstdint>

#ifndef SOMEIP_PLATFORM_STM32

// host stub, TCP lives in LinuxTcpTransport and needs no lwIP type
namespace someIp {
struct TcpConnection {};
} // namespace someIp

#else // SOMEIP_PLATFORM_STM32

#include "lwip/tcp.h"
#include "someIp/communication/TcpipCoreLock.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp{


struct TcpConnection {
    struct tcp_pcb* pcb = nullptr;
    // fixed no-heap RX accumulation buffer for the framer
    std::array<uint8_t, config::MAX_TCP_RX_MESSAGE> inbuf{};
    size_t expectedLen = 0;
    size_t receivedLen = 0;
    struct {
        pbuf* p = nullptr;
        u32_t sent  = 0;
        u32_t acked = 0;
        bool  active = false;
    } tx;



    TcpConnection() = default;

    explicit TcpConnection(uint16_t /*port*/) {
        TcpipCoreLock lock;
        pcb = tcp_new();
    }


    TcpConnection &operator=(TcpConnection &&ref_other) noexcept {
        if (this == &ref_other) return *this;

        // release owned pcb/tx pbuf before overwriting to avoid a leak
        if (pcb != nullptr || tx.p != nullptr) {
            TcpipCoreLock lock;
            if (pcb != nullptr) {
                tcp_close(pcb);
                pcb = nullptr;
            }
            if (tx.p != nullptr) {
                pbuf_free(tx.p);
                tx.p = nullptr;
            }
        }

        pcb = ref_other.pcb;
        ref_other.pcb = nullptr;

        inbuf = ref_other.inbuf;
        expectedLen = ref_other.expectedLen;
        receivedLen = ref_other.receivedLen;
        tx = ref_other.tx;
        ref_other.tx.p = nullptr;
        ref_other.tx.active = false;

        return *this;
    }

    ~TcpConnection() {
        // only lock when we own resources, the core may already be gone
        if (pcb == nullptr && tx.p == nullptr) return;
        TcpipCoreLock lock;
        if (pcb != nullptr) {
            tcp_close(pcb);
            pcb = nullptr;
        }
        if (tx.p != nullptr) {
            pbuf_free(tx.p); // free unsent zero-copy tx chain
            tx.p = nullptr;
        }
    }

};

}

#endif // SOMEIP_PLATFORM_STM32

#endif // SOMEIP_TCP_CONNECTION_HPP
