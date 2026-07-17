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

#include <thread>
#include <chrono>
#include <gtest/gtest.h>

#include <cstring>
#include <vector>
#include <memory>
#include <algorithm>

#include "someIp/utils/NetworkByteOrderConverter.hpp"
#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/Header.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/transport-protocol/Reassembler.hpp"
#include "someIp/transport-protocol/Segmenter.hpp"
#include "someIp/transport-protocol/TpPackage.hpp"
#include "someIp/enums/MessageType.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/ESomeIp.hpp"

static someIp::IpAddr makeIp4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return someIp::IpAddr::from_u32(((uint32_t)a << 24) | ((uint32_t)b << 16) |
                                    ((uint32_t)c << 8) | d);
}

// struct Captured {
//     void* arg{};
//     std::vector<std::vector<char>> packets;
// };

// static Captured* g_cap_sink = nullptr;

// namespace someIp {

// } 

// struct WithSink {
//     Captured cap{};
//     WithSink()  { g_cap_sink = &cap; }
//     ~WithSink() { g_cap_sink = nullptr; }
// };

TEST(Reassembler, SinglePdu_NoTpFlag) {
    // WithSink sink;

    someIp::Header hdr{};
    hdr._message_type          = MessageType::REQUEST;
    hdr._return_code           = ReturnCode::E_OK;
    hdr._request_id.client_id  = 42;
    hdr._request_id.session_id = 99u;
    hdr._message_id._service_id = 0x1234;
    hdr._message_id._method_id  = 0x5678;
    hdr._protocol_version      = 1;
    hdr._interface_version     = 1;
    hdr._length                = 8 + 5;

    someIp::Header net = hdr;
    net._message_id._service_id = someIp::ensure_network_order(net._message_id._service_id);
    net._message_id._method_id  = someIp::ensure_network_order(net._message_id._method_id);
    net._request_id.client_id  = someIp::ensure_network_order(net._request_id.client_id);
    net._request_id.session_id = someIp::ensure_network_order(net._request_id.session_id);
    net._length                = someIp::ensure_network_order(net._length);

    // one flat frame: header bytes followed by the body
    const char body[] = "HELLO";
    someIp::PacketBuffer frame(&net, sizeof(net));
    frame.append(body, sizeof(body) - 1);
    ASSERT_EQ(frame.size(), sizeof(someIp::Header) + sizeof(body) - 1);

    someIp::PackageRx rx(
        std::move(frame),
        makeIp4(1,2,3,4),
        1234
    );

    someIp::tp::Reassembler r;
    bool delivered = r.pushFragment(std::move(rx), /*arg*/nullptr);
    ASSERT_TRUE(delivered);
    // ASSERT_EQ(sink.cap.packets.size(), 1u);

    // auto& flat = sink.cap.packets[0];
    // ASSERT_EQ(flat.size(), sizeof(someIp::Header) + sizeof(body) - 1);
    // EXPECT_EQ(0, std::memcmp(flat.data(), &net, sizeof(someIp::Header)));
    // EXPECT_EQ(0, std::memcmp(flat.data() + sizeof(someIp::Header), body, sizeof(body) - 1));
}

TEST(Reassembler, TwoSegment_ReassemblySucceeds) {
    // WithSink  sink;

    std::string word = "HELLO!";
    std::string data;
    for (int i = 0; i < 433; ++i) data += word;

    someIp::Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 7;
    base._request_id.session_id = 55;
    base._message_id._service_id = 0x1111;
    base._message_id._method_id  = 0x2222;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::PacketBuffer wrap(data.data(), data.size());

    someIp::tp::Segmenter seg;
    auto ip4 = makeIp4(10,0,0,99);
    uint16_t port = 4242;

    auto s1 = seg.segment_next(base, wrap, 0, data.size(), ip4, port);
    uint32_t offset = s1.payload.size();
    auto s2 = seg.segment_next(base, wrap, offset, data.size(), ip4, port);

    auto p1 = s1.serialize();
    auto p2 = s2.serialize();

    someIp::tp::Reassembler r;

    bool d1 = r.pushFragment(
        someIp::PackageRx(std::move(p1), ip4, port),
        nullptr
    );
    EXPECT_FALSE(d1);

    bool d2 = r.pushFragment(
        someIp::PackageRx(std::move(p2), ip4, port),
        nullptr
    );
    ASSERT_TRUE(d2);
    // ASSERT_EQ(sink.cap.packets.size(), 1u);

    // auto& flat = sink.cap.packets[0];
    // ASSERT_GE(flat.size(), sizeof(someIp::Header));

    // someIp::Header netHdr{};
    // std::memcpy(&netHdr, flat.data(), sizeof(netHdr));
    // someIp::Header hostHdr = netHdr;
    // hostHdr.message_id.service_id = someIp::ensure_host_order(hostHdr.message_id.service_id);
    // hostHdr.message_id.method_id  = someIp::ensure_host_order(hostHdr.message_id.method_id);
    // hostHdr.request_id.client_id  = someIp::ensure_host_order(hostHdr.request_id.client_id);
    // hostHdr.request_id.session_id = someIp::ensure_host_order(hostHdr.request_id.session_id);
    // hostHdr.length                = someIp::ensure_host_order(hostHdr.length);

    // EXPECT_EQ(hostHdr.request_id.session_id, 55u);
    // EXPECT_EQ(hostHdr.length, uint32_t(8 + data.size()));

    // ASSERT_EQ(flat.size(), sizeof(someIp::Header) + data.size());
    // EXPECT_EQ(0, std::memcmp(flat.data() + sizeof(someIp::Header), data.data(), data.size()));
}

TEST(Reassembler, SameSessionIdDifferentSourceDoesNotMerge) {
    std::string word = "HELLO!";
    std::string data;
    for (int i = 0; i < 433; ++i) data += word;

    someIp::Header base{};
    base._message_type           = MessageType::REQUEST;
    base._return_code            = ReturnCode::E_OK;
    base._request_id.client_id   = 7;
    base._request_id.session_id  = 55;
    base._message_id._service_id = 0x1111;
    base._message_id._method_id  = 0x2222;
    base._protocol_version       = 1;
    base._interface_version      = 1;

    someIp::PacketBuffer wrap(data.data(), data.size());

    someIp::tp::Segmenter seg;
    auto ipA = makeIp4(10, 0, 0, 1);
    auto ipB = makeIp4(10, 0, 0, 2);

    auto a1 = seg.segment_next(base, wrap, 0, data.size(), ipA, 4242);
    auto b1 = seg.segment_next(base, wrap, 0, data.size(), ipB, 4243);
    auto pa1 = a1.serialize();
    auto pb1 = b1.serialize();

    someIp::tp::Reassembler r;
    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(pa1), ipA, 4242), nullptr));
    EXPECT_EQ(r.active_session_count(), 1u);
    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(pb1), ipB, 4243), nullptr));
    EXPECT_EQ(r.active_session_count(), 2u);
}

TEST(Reassembler, StalledSessionTimesOutOnScheduler) {
    std::string word = "HELLO!";
    std::string data;
    for (int i = 0; i < 433; ++i) data += word;

    someIp::Header base{};
    base._message_type           = MessageType::REQUEST;
    base._return_code            = ReturnCode::E_OK;
    base._request_id.client_id   = 7;
    base._request_id.session_id  = 55;
    base._message_id._service_id = 0x1111;
    base._message_id._method_id  = 0x2222;
    base._protocol_version       = 1;
    base._interface_version      = 1;

    someIp::PacketBuffer wrap(data.data(), data.size());

    someIp::tp::Segmenter seg;
    auto ip4 = makeIp4(10, 0, 0, 99);
    uint16_t port = 4242;
    auto s1 = seg.segment_next(base, wrap, 0, data.size(), ip4, port);
    auto p1 = s1.serialize();

    TimerScheduler scheduler;
    scheduler.start();

    someIp::tp::Reassembler r;
    r.set_scheduler(&scheduler);

    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(p1), ip4, port), nullptr));
    EXPECT_EQ(r.active_session_count(), 1u);

    // TP_RX_TIMEOUT_MS is about 201ms so wait it out
    bool cleaned = false;
    for (int i = 0; i < 100 && !cleaned; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cleaned = (r.active_session_count() == 0);
    }
    EXPECT_TRUE(cleaned) << "stalled session was not cleaned up by the RX timeout";

    scheduler.stop();
}

TEST(Reassembler, OutOfOrder_ReassemblesRegardlessOfArrivalOrder) {
    std::string data;
    for (int i = 0; i < 100; ++i) {
        for (unsigned char c = 'A'; c <= 'Z'; ++c) { data.push_back(c); }
    }

    someIp::Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 1;
    base._request_id.session_id = 77;
    base._message_id._service_id = 1;
    base._message_id._method_id  = 2;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::PacketBuffer w(data.data(), data.size());

    someIp::tp::Segmenter seg;
    auto ip4 = makeIp4(127,0,0,1);
    uint16_t port = 1111;

    auto s1      = seg.segment_next(base, w, 0,   data.size(), ip4, port);
    uint32_t off = s1.payload.size();
    auto s2      = seg.segment_next(base, w, off, data.size(), ip4, port);

    auto p1 = s1.serialize();
    auto p2 = s2.serialize();

    someIp::tp::Reassembler r;

    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(p2), ip4, port), nullptr));
    ASSERT_TRUE(r.pushFragment(someIp::PackageRx(std::move(p1), ip4, port), nullptr));
}

TEST(Reassembler, ThreeSegment_ReassemblySucceeds) {
    // WithSink sink;

    std::string data3;
    for (int i = 0; i < 156; ++i)
        for (unsigned char c = 'A'; c <= 'Z'; ++c) data3.push_back(c);

    someIp::Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 69;
    base._request_id.session_id = 420;
    base._message_id._service_id = 76;
    base._message_id._method_id  = 43;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::PacketBuffer w(data3.data(), data3.size());

    someIp::tp::Segmenter seg;
    auto ip4 = makeIp4(167,0,0,1);
    uint16_t port = 3030;

    auto s1 = seg.segment_next(base, w, 0,                      data3.size(), ip4, port);
    uint32_t off1 = s1.payload.size();
    auto s2 = seg.segment_next(base, w, off1,                   data3.size(), ip4, port);
    uint32_t off2 = off1 + s2.payload.size();
    auto s3 = seg.segment_next(base, w, off2,                   data3.size(), ip4, port);

    auto p1 = s1.serialize();
    auto p2 = s2.serialize();
    auto p3 = s3.serialize();

    someIp::tp::Reassembler r;

    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(p1), ip4, port), nullptr));
    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(p2), ip4, port), nullptr));
    ASSERT_TRUE (r.pushFragment(someIp::PackageRx(std::move(p3), ip4, port), nullptr));

    // ASSERT_EQ(sink.cap.packets.size(), 1u);
    // auto& flat = sink.cap.packets[0];
    // ASSERT_EQ(flat.size(), sizeof(someIp::Header) + data3.size());
    // EXPECT_EQ(0, std::memcmp(flat.data() + sizeof(someIp::Header), data3.data(), data3.size()));
}

TEST(Reassembler, HeaderChanged_MidStream_Aborts) {
    // WithSink sink;

    std::string data;
    for (int i = 0; i < 107; ++i)
        for (unsigned char c = 'A'; c <= 'Z'; ++c) data.push_back(c);

    someIp::Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 5;
    base._request_id.session_id = 200;
    base._message_id._service_id = 0xAAAA;
    base._message_id._method_id  = 0xBBBB;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::PacketBuffer w(data.data(), data.size());

    someIp::tp::Segmenter seg;
    auto ip4 = makeIp4(8,8,8,8);
    uint16_t port = 2020;

    auto s1 = seg.segment_next(base, w, 0,     data.size(), ip4, port);
    uint32_t off = s1.payload.size();
    auto s2 = seg.segment_next(base, w, off,   data.size(), ip4, port);
    auto s3 = seg.segment_next(base, w, off + s2.payload.size(), data.size(), ip4, port);

    auto p1 = s1.serialize();
    auto p2 = s2.serialize();
    auto p3 = s3.serialize();

    // corrupt the header bytes in place, before handing the buffer over
    someIp::Header corrupt{};
    ASSERT_GE(p2.size(), sizeof(corrupt));
    std::memcpy(&corrupt, p2.data(), sizeof(corrupt));
    corrupt._interface_version = 0xFF;
    std::memcpy(p2.data(), &corrupt, sizeof(corrupt));

    someIp::tp::Reassembler r;

    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(p1), ip4, port), nullptr));
    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(p2), ip4, port), nullptr));
    EXPECT_FALSE(r.pushFragment(someIp::PackageRx(std::move(p3), ip4, port), nullptr));

    // EXPECT_TRUE(sink.cap.packets.empty());
}
