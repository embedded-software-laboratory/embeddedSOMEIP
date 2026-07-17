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

#ifndef DESERIALIZER_HPP_
#define DESERIALIZER_HPP_

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>

namespace someIp
{
    // reads from a caller-owned byte range (e.g. a pbuf payload) - no copy/heap
    class Deserializer
    {
    private:
        const uint8_t *m_ptr_data;
        size_t m_size;
        size_t m_pos = 0;

    public:
        Deserializer(const uint8_t *ptr_data, size_t size);

        // deserialize an 8-bit value into two 4-bit values
        bool deserialize_4(uint8_t value[2]);

        bool deserialize(uint8_t &ref_value);

        bool deserialize(uint16_t &ref_value);

        bool deserialize(uint32_t &ref_value);

        bool deserialize(std::string &ref_value, uint8_t length);

        bool deserialize_24(uint32_t &ref_value);

        bool deserialize(std::array<uint8_t, 16> &ref_ipv6_address);

        bool dump(uint8_t num);

        bool peak(uint8_t &ref_value, uint8_t num);

        // bytes still unread
        size_t remaining() const { return m_size - m_pos; }
    };
} // namespace someIp
#endif // DESERIALIZER_HPP_