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

#include <array>

#include "Serializer.hpp"
#include "NetworkByteOrderConverter.hpp"

namespace someIp
{
    void Serializer::serialize_4(const uint8_t value[2])
    {
        uint8_t tmp = static_cast<uint8_t>(value[0] & 0x0F) | static_cast<uint8_t>(value[1] << 4);
        m_buffer.push_back(tmp);
    }

    void Serializer::serialize(uint8_t value)
    {
        m_buffer.push_back(value);
    }

    void Serializer::serialize(uint16_t value)
    {
        // shifts, not ensure_network_order, which double-converts on LE
        m_buffer.push_back(static_cast<uint8_t>(value >> 8));
        m_buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    void Serializer::serialize(uint32_t value)
    {
        // big-endian via shifts, no ensure_network_order
        m_buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    void Serializer::serialize(const std::string &ref_value)
    {
        for (char c : ref_value) m_buffer.push_back(static_cast<uint8_t>(c));
    }

    void Serializer::serialize_24(uint32_t value)
    {
        // low 24 bits big-endian, used for the SD entry TTL
        m_buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    void Serializer::serialize(const std::array<uint8_t, 16> &ref_ipv6_address)
    {
        for (uint8_t b : ref_ipv6_address) m_buffer.push_back(b);
    }
} // namespace someIp
