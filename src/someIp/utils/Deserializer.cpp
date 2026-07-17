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
#include <cstring>

#include "Deserializer.hpp"
#include "NetworkByteOrderConverter.hpp"

namespace someIp
{
    Deserializer::Deserializer(const uint8_t *ptr_data, size_t size) : m_ptr_data(ptr_data), m_size(size) {}

    bool Deserializer::deserialize_4(uint8_t value[2])
    {
        if (m_pos >= m_size)
        {
            return false;
        }
        uint8_t tmp = m_ptr_data[m_pos++];
        value[0] = static_cast<uint8_t>(tmp & 0x0F);
        value[1] = static_cast<uint8_t>(tmp >> 4);
        return true;
    }

    bool Deserializer::deserialize(uint8_t &ref_value)
    {
        if (m_pos >= m_size)
        {
            return false;
        }
        ref_value = m_ptr_data[m_pos++];
        return true;
    }

    bool Deserializer::deserialize(uint16_t &ref_value)
    {
        if (m_size - m_pos < 2)
        {
            return false;
        }
        // big-endian via shifts, no ensure_host_order (mirror of serialize fix)
        ref_value = static_cast<uint16_t>(m_ptr_data[m_pos++]) << 8;
        ref_value |= static_cast<uint16_t>(m_ptr_data[m_pos++]);
        return true;
    }

    bool Deserializer::deserialize(uint32_t &ref_value)
    {
        if (m_size - m_pos < 4)
        {
            return false;
        }
        // big-endian via shifts, no ensure_host_order
        ref_value = static_cast<uint32_t>(m_ptr_data[m_pos++]) << 24;
        ref_value |= static_cast<uint32_t>(m_ptr_data[m_pos++]) << 16;
        ref_value |= static_cast<uint32_t>(m_ptr_data[m_pos++]) << 8;
        ref_value |= static_cast<uint32_t>(m_ptr_data[m_pos++]);
        return true;
    }

    bool Deserializer::deserialize(std::string &ref_value, uint8_t length)
    {
        if (m_size - m_pos < length)
        {
            return false;
        }
        ref_value.assign(reinterpret_cast<const char *>(m_ptr_data + m_pos), length);
        m_pos += length;
        return true;
    }

    bool Deserializer::deserialize_24(uint32_t &ref_value)
    {
        if (m_size - m_pos < 3)
        {
            return false;
        }
        // read 3 big-endian bytes into the low 24 bits, no host-order conversion
        ref_value = 0;
        ref_value |= static_cast<uint32_t>(m_ptr_data[m_pos++]) << 16;
        ref_value |= static_cast<uint32_t>(m_ptr_data[m_pos++]) << 8;
        ref_value |= static_cast<uint32_t>(m_ptr_data[m_pos++]);
        return true;
    }

    bool Deserializer::deserialize(std::array<uint8_t, 16> &ref_ipv6_address)
    {
        if (m_size - m_pos < 16)
        {
            return false;
        }
        std::memcpy(ref_ipv6_address.data(), m_ptr_data + m_pos, 16);
        m_pos += 16;
        return true;
    }

    bool Deserializer::dump(uint8_t num)
    {
        if (m_size - m_pos < num)
        {
            return false;
        }
        m_pos += num;
        return true;
    }

    bool Deserializer::peak(uint8_t &ref_value, uint8_t num)
    {
        if (m_size - m_pos < num)
        {
            return false;
        }
        ref_value = m_ptr_data[m_pos + num - 1];
        return true;
    }
} // namespace someIp
