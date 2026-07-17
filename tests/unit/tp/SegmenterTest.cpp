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

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <vector>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/structs/Header.hpp"
#include "someIp/enums/MessageType.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/transport-protocol/Segmenter.hpp"
#include "someIp/transport-protocol/TpPackage.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"

using someIp::tp::Segmenter;
using someIp::tp::TpPackage;
using someIp::tp::TpHeader;
using someIp::tp::TpOnlyHeader;
using someIp::Header;

// serialize() prepends the wire header, so the payload starts this far in
static constexpr size_t TP_HEADER_SIZE = sizeof(TpHeader);

// someIp IpAddr stores the address in HOST order (unlike lwIPs IP4_ADDR)
static someIp::IpAddr makeip(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return someIp::IpAddr::from_u32(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d);
}


TEST(slicer_PacketBuffer, SliceCopyValid) // 1
{
    const char pattern [] = "abcdefghijklmnopqrstuvwxyz";
    size_t total_len = strlen(pattern);
    someIp::PacketBuffer buf(pattern, total_len);

    uint32_t length = 10;
    uint32_t offset = 5;
    someIp::PacketBuffer slice = buf.slice_copy(length, offset);

    ASSERT_FALSE(slice.empty());
    EXPECT_EQ(slice.size(), length);
    EXPECT_EQ(std::memcmp(slice.data(), pattern + offset, length), 0) << "slice_copy data mismatch";

    EXPECT_EQ(buf.size(), total_len);
}




TEST(slicer_PacketBuffer, SliceCopyInvalidArgs) //2
{
    someIp::PacketBuffer buf(5);

    someIp::PacketBuffer slice1 = buf.slice_copy(0,0);
    EXPECT_TRUE(slice1.empty());

    someIp::PacketBuffer slice2 = buf.slice_copy(10,0);
    EXPECT_TRUE(slice2.empty());

    someIp::PacketBuffer slice3 = buf.slice_copy(1,6);
    EXPECT_TRUE(slice3.empty());
}




TEST(slicer_PacketBuffer, SliceMoveExactSize) //3
{
    // payload larger than 256 bytes
    const char data[] = "helojkfdjksdnmvoihjnlihfbabjlknagsdrpjoigsdabjnlkäsfbaphirgasbnlkjgfsahpibn#klwegphoiagsfpi#hgasjhpoigahjpio#hjipogsnk#vyihphpi#rgahpiwrgnbhkjpigpihgsrfajklösdfakjölfdsakjlöfsdakjlödfsalkjfsdalkjöfdsakjlöfsdalköjfsdaölkjfsdaljköfdsaökljfsdaökljsdfaölkjfsdalökjfdsalöjkfsdalökjfdsalköjfdsalökjfdsajlköööööööööööööööööjlökfdasjklöfadsjklöfdasjklöfadskjlökjlöjlkalkjsdfiaweirpghfasdkfvnaälksghoiwgnbyljsdvnaosrhgiaänsdlgkähansdlmvnaälskdhjgfnälasdkvnüapäsidhgnäalöksdjväapiersgnäpaskdjvalöskdhjgapäsikdnf#äpylsdvmaöäslkhgfpaiknsdväpajsdiogfhalwekrngpaoisudgpihasd#pgfkha#posrhgp#aiwerngälyksfhg#+pasorjhdg#paihg#pia";
    size_t total_len = strlen(data);
    someIp::PacketBuffer buf(data, total_len);

    someIp::PacketBuffer moved = buf.slice_move((uint32_t)total_len);
    ASSERT_FALSE(moved.empty());
    EXPECT_EQ(moved.size(), total_len);
    EXPECT_EQ(std::memcmp(moved.data(), data, total_len), 0) << "slice_move (exact) data mismatch";

    // slice_move only advances the front cursor, the source still owns its memory
    EXPECT_TRUE(buf.empty());
    EXPECT_FALSE(static_cast<bool>(buf));
}




TEST(slicer_PacketBuffer, SliceMovePartialWithinSingleBuffer)//4
{
    const char data[] = "0123456789";
    size_t total_len = strlen(data);
    someIp::PacketBuffer buf(data, total_len);

    uint32_t length = 4;
    someIp::PacketBuffer moved = buf.slice_move(length);
    ASSERT_FALSE(moved.empty());
    EXPECT_EQ(moved.size(), length);
    EXPECT_EQ(std::memcmp(moved.data(), data, length), 0) << "slice_move (partial single) data mismatch";

    ASSERT_FALSE(buf.empty());
    EXPECT_EQ(buf.size(), total_len - length);
    EXPECT_EQ(std::memcmp(buf.data(), data + length, buf.size()),0) << "slice_move (partial single) remainder mismatch";
}



TEST(slicer_PacketBuffer, huge_test)//4
{
    std::string data;
    std::string word = "hallo!!";
    for(int i = 0; i<800; i++) // 5600 bytes
    {
        data+= word;
    }
    size_t total_len = data.size();
    someIp::PacketBuffer buf(data.data(), total_len);

    uint32_t length = 2800;
    someIp::PacketBuffer moved = buf.slice_copy(length,0);
    ASSERT_FALSE(moved.empty());
    EXPECT_EQ(moved.size(), length);
    EXPECT_EQ(std::memcmp(moved.data(), data.data(), length), 0) << "slice_copy data mismatch";

    // slice_copy leaves the source untouched, remainder is the whole buffer
    ASSERT_FALSE(buf.empty());
    EXPECT_EQ(buf.size(), total_len);
    EXPECT_EQ(std::memcmp(buf.data(), data.data(), total_len),0) << "slice_copy must leave the source intact";
}




TEST(slicer_PacketBuffer, SliceMovePartialAcrossLastPart) //5
{
    const char data[] = "ABCDEFGHIJ";
    someIp::PacketBuffer buf(data, strlen(data));

    uint32_t length = 8;
    someIp::PacketBuffer moved = buf.slice_move(length);
    ASSERT_FALSE(moved.empty());

    const char expected1[] = "ABCDEFGH";
    EXPECT_EQ(moved.size(), length);
    {
        char bufMoved[9] = {0};
        EXPECT_EQ(moved.copy_out(bufMoved, length, 0), length);
        EXPECT_STREQ(bufMoved, expected1) << "slice_move (partial) data mismatch";
    }

    ASSERT_FALSE(buf.empty());
    EXPECT_EQ(buf.size(), 2u);
    {
        char bufRem[3] = {0};
        EXPECT_EQ(buf.copy_out(bufRem, 2, 0), 2u);
        EXPECT_STREQ(bufRem, "IJ") << "slice_move (partial) remainder mismatch";
    }
}




TEST(slicer_PacketBuffer, SliceMovePartialAcrossMiddle) //6
{
    const char data[] = "ABCDEFGHIJKLMNO";
    someIp::PacketBuffer buf(data, strlen(data));

    uint32_t length = 8;
    someIp::PacketBuffer moved = buf.slice_move(length);
    ASSERT_FALSE(moved.empty());

    const char expected1[] = "ABCDEFGH";
    EXPECT_EQ(moved.size(), length);
    {
        char bufMoved[9] = {0};
        EXPECT_EQ(moved.copy_out(bufMoved, length, 0), length);
        EXPECT_STREQ(bufMoved, expected1) << "slice_move (partial) data mismatch";
    }

    ASSERT_FALSE(buf.empty());
    EXPECT_EQ(buf.size(), 7u);
    {
        char bufRem[8] = {0};
        EXPECT_EQ(buf.copy_out(bufRem, 7, 0), 7u);
        EXPECT_STREQ(bufRem, "IJKLMNO") << "slice_move (partial) remainder mismatch";
    }
}




TEST(slicer_PacketBuffer, SliceMovePartialAlligned) //7
{
    const char data[] = "ABCDEFGHIJ";
    someIp::PacketBuffer buf(data, strlen(data));

    // 5 lands exactly on the former chain boundary
    uint32_t length = 5;
    someIp::PacketBuffer moved = buf.slice_move(length);
    ASSERT_FALSE(moved.empty());

    const char expected1[] = "ABCDE";
    EXPECT_EQ(moved.size(), length);
    {
        char bufMoved[6] = {0};
        EXPECT_EQ(moved.copy_out(bufMoved, length, 0), length);
        EXPECT_STREQ(bufMoved, expected1) << "slice_move (aligned) data mismatch";
    }

    ASSERT_FALSE(buf.empty());
    EXPECT_EQ(buf.size(), 5u);
    {
        char bufRem[6] = {0};
        EXPECT_EQ(buf.copy_out(bufRem, 5, 0), 5u);
        EXPECT_STREQ(bufRem, "FGHIJ") << "slice_move (aligned) remainder mismatch";
    }
}



TEST(SegmenterTest, SingleSegmentNoAllign) // 1
{
    const char data[] = "12345";
    size_t total_length = strlen(data);
    someIp::PacketBuffer buf(data, total_length);

    char originalBuf[6] = {0};
    buf.copy_out(originalBuf, total_length, 0);

    Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 1;
    base._request_id.session_id = 2;
    base._message_id._service_id = 3;
    base._message_id._method_id  = 4;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::IpAddr ip = makeip(192,168,0,1);
    uint16_t port   = 1234;
    uint32_t offset = 0;

    // 28 minus the 20-byte wire header leaves 8, so 5 fits in one segment
    Segmenter segmenter; segmenter.set_max_segment_size(28);
    TpPackage seg = segmenter.segment_next(base, buf, offset, (uint32_t)total_length, ip, port);

    EXPECT_EQ((static_cast<uint8_t>(seg.tpheader.message_type) & 0x20), 0x20);
    EXPECT_EQ(seg.tpheader.tponlyheader.get_offset(), 0);
    EXPECT_FALSE(seg.tpheader.tponlyheader.get_msf());

    auto chain = seg.serialize();
    ASSERT_FALSE(chain.empty());
    EXPECT_EQ(chain.size(), TP_HEADER_SIZE + total_length);
    {
        char payloadBuf[6] = {0};
        EXPECT_EQ(chain.copy_out(payloadBuf, total_length, TP_HEADER_SIZE), total_length);
        EXPECT_STREQ(payloadBuf,data);
    }

    ASSERT_FALSE(buf.empty());
    EXPECT_EQ(buf.size(), total_length);
    {
        char bufRem[6] = {0};
        EXPECT_EQ(buf.copy_out(bufRem, total_length, 0), total_length);
        EXPECT_STREQ(bufRem, originalBuf) << "After segment_next, the buffers contents should not have changed";
    }
}




TEST(SegmenterTest, SingleSegmentNoAllign_MOVE) // 1
{
    const char data[] = "12345";
    size_t total_length = strlen(data);
    someIp::PacketBuffer buf(data, total_length);

    Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 1;
    base._request_id.session_id = 2;
    base._message_id._service_id = 3;
    base._message_id._method_id  = 4;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::IpAddr ip = makeip(192,168,0,1);
    uint16_t port   = 1234;
    uint32_t offset = 0;

    // 28 minus the 20-byte wire header leaves 8, so 5 fits in one segment
    Segmenter segmenter; segmenter.set_max_segment_size(28);
    TpPackage seg = segmenter.segment_next_move(base, buf, offset, (uint32_t)total_length, ip, port);

    EXPECT_EQ((static_cast<uint8_t>(seg.tpheader.message_type) & 0x20), 0x20);
    EXPECT_EQ(seg.tpheader.tponlyheader.get_offset(), 0);
    EXPECT_FALSE(seg.tpheader.tponlyheader.get_msf());

    auto chain = seg.serialize();
    ASSERT_FALSE(chain.empty());
    EXPECT_EQ(chain.size(), TP_HEADER_SIZE + total_length);
    {
        char payloadBuf[6] = {0};
        EXPECT_EQ(chain.copy_out(payloadBuf, total_length, TP_HEADER_SIZE), total_length);
        EXPECT_STREQ(payloadBuf,data);
    }

    EXPECT_TRUE(buf.empty()) << "after segment_next_move of the entire buffer the source must be drained";
}




TEST(SegmenterTest, MultipleSegmentsAligned) // 2
{
    // max segment 37 forces 16 byte aligned segments
    std::string payload(20, 'A');
    someIp::PacketBuffer buf(payload.data(), payload.size());

    std::vector<char> original(20);
    buf.copy_out(original.data(), 20, 0);

    Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 1;
    base._request_id.session_id = 2;
    base._message_id._service_id = 3;
    base._message_id._method_id  = 4;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::IpAddr ip = makeip(10,0,0,1);
    uint16_t port   = 4321;
    uint32_t total_length = (uint32_t)payload.size();

    Segmenter segmenter; segmenter.set_max_segment_size(37);
    TpPackage seg1 = segmenter.segment_next(base, buf, 0, total_length, ip, port);
    EXPECT_EQ((static_cast<uint8_t>(seg1.tpheader.message_type) & 0x20), 0x20);
    EXPECT_EQ(seg1.tpheader.tponlyheader.get_offset(), 0);
    EXPECT_TRUE(seg1.tpheader.tponlyheader.get_msf());

    auto chain1 = seg1.serialize();
    ASSERT_FALSE(chain1.empty());
    EXPECT_EQ(chain1.size(), TP_HEADER_SIZE + 16u);
    {
        char buf1[17] = {0};
        EXPECT_EQ(chain1.copy_out(buf1, 16, TP_HEADER_SIZE), 16u);
        EXPECT_EQ(std::string(buf1, 16), std::string(16,'A'));
    }

    TpPackage seg2 = segmenter.segment_next(base, buf, 16, total_length, ip, port);
    EXPECT_EQ((static_cast<uint8_t>(seg2.tpheader.message_type) & 0x20), 0x20);
    EXPECT_EQ(seg2.tpheader.tponlyheader.get_offset(),1);
    EXPECT_FALSE(seg2.tpheader.tponlyheader.get_msf());

    auto chain2 = seg2.serialize();
    ASSERT_FALSE(chain2.empty());
    EXPECT_EQ(chain2.size(), TP_HEADER_SIZE + 4u);
    {
        char buf2[5] = {0};
        EXPECT_EQ(chain2.copy_out(buf2, 4, TP_HEADER_SIZE), 4u);
        EXPECT_EQ(std::string(buf2, 4), std::string(4,'A'));
    }

    ASSERT_FALSE(buf.empty());
    EXPECT_EQ(buf.size(), 20u);
    {
        std::vector<char> postContents(20);
        EXPECT_EQ(buf.copy_out(postContents.data(), 20, 0), 20u);
        EXPECT_EQ(postContents, original);
    }
}




TEST(SegmenterTest, MultipleSegmentsAligned_MOVE) // 2
{
    // max segment 37 forces 16 byte aligned segments
    std::string payload(20, 'A');
    someIp::PacketBuffer buf(payload.data(), payload.size());

    Header base{};
    base._message_type          = MessageType::REQUEST;
    base._return_code           = ReturnCode::E_OK;
    base._request_id.client_id  = 1;
    base._request_id.session_id = 2;
    base._message_id._service_id = 3;
    base._message_id._method_id  = 4;
    base._protocol_version      = 1;
    base._interface_version     = 1;

    someIp::IpAddr ip = makeip(10,0,0,1);
    uint16_t port   = 4321;
    uint32_t total_length = (uint32_t)payload.size();

    // 37 minus the 20-byte wire header leaves 17, aligned down to 16
    Segmenter segmenter; segmenter.set_max_segment_size(37);
    TpPackage seg1 = segmenter.segment_next_move(base, buf, 0, total_length, ip, port);
    EXPECT_EQ((static_cast<uint8_t>(seg1.tpheader.message_type) & 0x20), 0x20);
    EXPECT_EQ(seg1.tpheader.tponlyheader.get_offset(), 0);
    EXPECT_TRUE(seg1.tpheader.tponlyheader.get_msf());

    auto chain1 = seg1.serialize();
    ASSERT_FALSE(chain1.empty());
    EXPECT_EQ(chain1.size(), TP_HEADER_SIZE + 16u);
    {
        char buf1[17] = {0};
        EXPECT_EQ(chain1.copy_out(buf1, 16, TP_HEADER_SIZE), 16u);
        EXPECT_EQ(std::string(buf1, 16), std::string(16,'A'));
    }

    {
        ASSERT_FALSE(buf.empty());
        EXPECT_EQ(buf.size(), 4u);
        char bufRem1[5] = {0};
        EXPECT_EQ(buf.copy_out(bufRem1, 4, 0), 4u);
        EXPECT_EQ(std::string(bufRem1, 4), std::string(4, 'A'));
    }

    TpPackage seg2 = segmenter.segment_next_move(base, buf, 16, total_length, ip, port);
    EXPECT_EQ((static_cast<uint8_t>(seg2.tpheader.message_type) & 0x20), 0x20);
    EXPECT_EQ(seg2.tpheader.tponlyheader.get_offset(),1); // offset unit is 16 bytes
    EXPECT_FALSE(seg2.tpheader.tponlyheader.get_msf());

    auto chain2 = seg2.serialize();
    ASSERT_FALSE(chain2.empty());
    EXPECT_EQ(chain2.size(), TP_HEADER_SIZE + 4u);
    {
        char buf2[5] = {0};
        EXPECT_EQ(chain2.copy_out(buf2, 4, TP_HEADER_SIZE), 4u);
        EXPECT_EQ(std::string(buf2, 4), std::string(4,'A'));
    }

    EXPECT_TRUE(buf.empty());
}
