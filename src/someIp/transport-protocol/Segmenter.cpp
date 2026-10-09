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

#include "Segmenter.hpp"

namespace someIp 
{
    namespace tp 
    {
        // slice the SDU into the next SOME/IP-TP segment [SWS_SomeIpTp_00006/00009]
        TpPackage Segmenter::segment_next(const Header &ref_header, PacketBuffer &ref_payload, uint32_t offset, uint32_t total_length, IpAddr& ref_destAddr, uint16_t destPort)
        {
            const uint32_t max_segment_size = m_maxSegmentSize;
            // PDU budget minus the 16-byte SOME/IP header and 4-byte TP field
            const uint32_t max_payload_per_segment = max_segment_size - static_cast<uint32_t>(sizeof(TpHeader));

            uint32_t remaining = total_length - offset;
            uint32_t chunk_size = std::min(remaining, max_payload_per_segment); // [SWS_SomeIpTp_00019]

            bool is_last_segment = (offset + chunk_size >= total_length);
            if (!is_last_segment) {
                // 16-byte align intermediate segments [SWS_SomeIpTp_00006]
                chunk_size = (chunk_size / 16) * 16;
            }

            PacketBuffer slice = ref_payload.slice_copy(chunk_size, offset);

            TpPackage segment(
                ref_header,
                std::move(slice),
                ref_destAddr,
                destPort,
                offset/16, // offset field is in units of 16 bytes
                !is_last_segment // MSF [SWS_SomeIpTp_00014], [SWS_SomeIpTp_00015]
            );

            // set TP-Flag (bit 5 of message type) [SWS_SomeIpTp_00009]
            segment.tpheader.message_type = static_cast<MessageType>(static_cast<uint8_t>(ref_header._message_type)|0x20);
            
            return segment;
        }





        TpPackage Segmenter::segment_next_move(const Header &ref_header, PacketBuffer &ref_payload, uint32_t offset, uint32_t total_length, IpAddr& ref_destAddr, uint16_t destPort)
        {
            const uint32_t max_segment_size = m_maxSegmentSize;
            // same budget as segment_next
            const uint32_t max_payload_per_segment = max_segment_size - static_cast<uint32_t>(sizeof(TpHeader));

            uint32_t remaining = total_length - offset;
            uint32_t chunk_size = std::min(remaining, max_payload_per_segment); // [SWS_SomeIpTp_00019]

            // [SWS_SomeIpTp_00006] All but the last segment must be 16-byte aligned
            bool is_last_segment = (offset + chunk_size >= total_length);
            if (!is_last_segment) {
                chunk_size = (chunk_size / 16) * 16;
            }

            PacketBuffer slice = ref_payload.slice_move(chunk_size); // [SWS_SomeIpTp_00078] slice_move returns an empty buffer on failure, callers do not check it yet

            TpPackage segment(
                ref_header,
                std::move(slice),
                ref_destAddr,
                destPort,
                offset/16,
                !is_last_segment // [SWS_SomeIpTp_00014], [SWS_SomeIpTp_00015]
            );
            //set tp flag [SWS_SomeIpTp_00009]
            segment.tpheader.message_type = static_cast<MessageType>(static_cast<uint8_t>(ref_header._message_type)|0x20);
        
            return segment;
        }

    } // namespace tp
} // namespace someIp
