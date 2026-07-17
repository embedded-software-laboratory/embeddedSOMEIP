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

#ifndef SOMEIP_TP_SEGMENTER_HPP
#define SOMEIP_TP_SEGMENTER_HPP

#include <memory>
#include "someIp/structs/Package.hpp"
#include "someIp/transport-protocol/TpPackage.hpp"
#include "someIp/transport-protocol/config/Tp_Config.hpp"
#include "someIp/pattern/Singleton.hpp"

// transport wrapper for TP, message structure in TpPackage


//pdur_someiptptransmit()

namespace someIp 
{
    namespace tp 
    {

        struct Segmenter
        {
        public:
            //TpPackage segment_next(Package& package, uint32_t offset, uint32_t total_length);
            TpPackage segment_next(const Header& ref_header, PacketBuffer& ref_payload, uint32_t offset, uint32_t total_length, IpAddr& ref_destAddr, uint16_t destPort);
            TpPackage segment_next_move(const Header &ref_header, PacketBuffer &ref_payload, uint32_t offset, uint32_t total_length, IpAddr& ref_destAddr, uint16_t destPort);

            // per-instance max TP PDU length, no global mutation
            void set_max_segment_size(uint32_t bytes) { m_maxSegmentSize = bytes; }
            uint32_t max_segment_size() const { return m_maxSegmentSize; }

            Segmenter() = default;
            ~Segmenter() = default;

        private:
            uint32_t m_maxSegmentSize = config::TP_PDU_LENGTH;
        };

    } // namespace tp
} // namespace someIp

#endif // SOMEIP_TP_SEGMENTER_HPP
