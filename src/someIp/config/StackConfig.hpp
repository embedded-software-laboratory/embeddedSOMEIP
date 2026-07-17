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

#ifndef SOMEIP_CONFIG_STACKCONFIG_HPP
#define SOMEIP_CONFIG_STACKCONFIG_HPP

#include <cstdint>
#include <cstddef>

// 1 = pool exhaustion aborts, 0 = log and degrade
#ifndef SOMEIP_POOL_EXHAUSTION_FATAL
#if defined(SOMEIP_PLATFORM_STM32)
#define SOMEIP_POOL_EXHAUSTION_FATAL 1
#else
#define SOMEIP_POOL_EXHAUSTION_FATAL 0
#endif
#endif

// sizes the whole SOME/IP stack footprint
namespace someIp {
namespace config {

// ----------------------------------------------------------------------------
// Connections, worker threads, queues
// ----------------------------------------------------------------------------
constexpr uint8_t SOMEIP_MAX_NUM_CONNECTIONS = 10;   // UDP/TCP connection pool

constexpr size_t NUMBER_OF_RECEIVER_THREADS = 1;
// must stay 1 so per-destination SD session ids leave in order
constexpr size_t NUMBER_OF_SENDER_THREADS   = 1;

constexpr size_t RECEIVER_QUEUE_LENGTH = 20;
constexpr size_t SENDER_QUEUE_LENGTH   = 20;

// 8 KB, SD serialization in a deep call chain overflows 4 KB
constexpr int RECEIVER_THREAD_SIZE = 8192;
constexpr int SENDER_THREAD_SIZE   = 4096;
// between the app tasks (24) and the lwIP tcpip thread (40)
constexpr int RECEIVER_THREAD_PRIO = 30;
constexpr int SENDER_THREAD_PRIO   = 30;

// event_manager worker (SD timers, TTL expiry)
constexpr int EVENT_MANAGER_THREAD_SIZE = 8192;
constexpr int EVENT_MANAGER_THREAD_PRIO = 3;

// SOME/IP-TP (segmentation / reassembly)
// concurrent reassembly / transmit sessions
constexpr size_t TP_RX_SESSIONS = 16;
constexpr size_t TP_TX_SESSIONS = 16;

// includes the 20-byte wire header, payload is this minus 20 [SWS_SomeIpTp_00004]
constexpr uint32_t TP_PDU_LENGTH         = 1392;
// max reassembled TP message, larger is rejected
constexpr size_t   MAX_TP_RX_MESSAGE     = 20480;
constexpr size_t   TP_REASSEMBLY_POOL_BLOCKS = 2; // concurrent in-flight reassemblies

// max SOME/IP message reassembled from a TCP stream, must stay <= 0xFFFF
constexpr size_t   MAX_TCP_RX_MESSAGE    = 20480;
constexpr uint16_t TP_BURST_SIZE         = 64;   // segments per burst
constexpr uint16_t TP_SEPARATION_TIME_MS = 0;    // gap between bursts [ms]
constexpr uint32_t TP_RX_TIMEOUT_MS      = TP_SEPARATION_TIME_MS + 200;

// Request/response and Service Discovery capacities
// outstanding request->response callbacks held at once
constexpr size_t MAX_OUTSTANDING_REQUESTS = 256;

// keep MAX_LOCAL_SERVICES <= MAX_REMOTE_SERVICES
constexpr size_t MAX_LOCAL_SERVICES             = 8;
constexpr size_t MAX_REMOTE_SERVICES            = 8;
constexpr size_t MAX_SUBSCRIBERS_PER_EVENTGROUP = 8;
constexpr size_t MAX_SERVICES_PER_ENDPOINT      = 8;
// per-service caps size the inline sd_service storage
constexpr size_t MAX_EVENTS_PER_SERVICE         = 16;
constexpr size_t MAX_EVENTGROUPS_PER_SERVICE    = 8;
constexpr size_t MAX_EVENTS_PER_EVENTGROUP      = 8;
constexpr size_t MAX_EVENT_PAYLOAD              = 64;
constexpr size_t MAX_ENTRIES_PER_SD_MESSAGE     = 16;
constexpr size_t MAX_OPTIONS_PER_SD_MESSAGE     = 16;
constexpr size_t MAX_TIMERS                     = 64;

// send current field values right after the ack [PRS_SOMEIPSD]
constexpr bool SD_INITIAL_NOTIFY_ON_SUBSCRIBE   = true;

// RxContext registries (per eSomeIP)
constexpr size_t MAX_RX_METHODS                 = 32;
constexpr size_t MAX_RX_EVENT_HANDLERS          = 32;

// value-owned service/method registry (ServiceHandler / Service)
constexpr size_t MAX_SERVICES                   = 16;
constexpr size_t MAX_METHODS_PER_SERVICE        = 16;


// block must hold the largest pooled SD entry or option
constexpr size_t SD_POOL_BLOCK_SIZE             = 384;
constexpr size_t SD_POOL_BLOCKS                 = 64;

// pool for sd_message objects (deserialized SD messages in flight)
constexpr size_t SD_LARGE_POOL_BLOCK_SIZE       = 768;
constexpr size_t SD_LARGE_POOL_BLOCKS           = 8;

// pool for sd_service objects, one block per discoverable remote service
constexpr size_t SD_SERVICE_POOL_BLOCK_SIZE     = 8192;
constexpr size_t SD_SERVICE_POOL_BLOCKS         = MAX_REMOTE_SERVICES;

// pool block size for scheduler timer_event nodes (count is MAX_TIMERS)
constexpr size_t TIMER_POOL_BLOCK_SIZE          = 256;
// inline storage for a timer callback (no heap) and its name
constexpr size_t TIMER_CB_SIZE                  = 96;
constexpr size_t TIMER_NAME_SIZE                = 24;
// max timers firing in one scheduler tick (caps run_loop stack scratch)
constexpr size_t MAX_TIMERS_PER_TICK            = 16;
// pending events queued per SD state machine
constexpr size_t MAX_FSM_EVENTS                 = 16;
// inline storage for a state machine's name (no heap)
constexpr size_t FSM_NAME_SIZE                  = 32;

// scratch buffer for serializing one SD message (no-heap, on the stack)
constexpr size_t SD_SERIALIZE_BUF               = 768;

// raw bytes of one SD configuration option's config string (no-heap)
constexpr size_t MAX_CONFIG_OPTION_BYTES        = 256;

// Non-allocating callback storage
// inline buffer for InplaceFunction, larger captures fail to compile
constexpr size_t CALLBACK_INPLACE_SIZE = 64;

// inline storage for a ByteBuffer backend, larger backends fail to compile
constexpr size_t BUFFER_INPLACE_SIZE = 64;

// inline storage for a concrete ITransport, larger backends fail to compile
constexpr size_t TRANSPORT_INPLACE_SIZE = 256;

} // namespace config
} // namespace someIp

#endif // SOMEIP_CONFIG_STACKCONFIG_HPP
