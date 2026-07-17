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

#ifndef SERIALIZER_HPP_
#define SERIALIZER_HPP_

#include <cstdint>
#include <array>
#include <string>

#include "someIp/utils/StaticVector.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp
{
    // serializes into a fixed inline buffer, bytes beyond SD_SERIALIZE_BUF are dropped
    class Serializer
    {
    private:
        StaticVector<uint8_t, config::SD_SERIALIZE_BUF> m_buffer;

    public:
        Serializer() = default;

        // packs the low nibbles of value[0] and value[1] into one byte
        void serialize_4(const uint8_t value[2]);

        void serialize(uint8_t value);

        void serialize(uint16_t value);

        void serialize(uint32_t value);

        void serialize(const std::string &ref_value);

        void serialize_24(uint32_t value);

        void serialize(const std::array<uint8_t, 16> &ref_ipv6_address);

        const uint8_t *data() const { return m_buffer.data(); }
        size_t size() const { return m_buffer.size(); }
    };
} // namespace someIp
#endif // SERIALIZER_HPP_