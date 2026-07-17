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

#ifndef SOMEIP_SD_LOAD_BALANCING_OPTION_HPP_
#define SOMEIP_SD_LOAD_BALANCING_OPTION_HPP_

#include <cstdint>
#include <vector>

#include "Option.hpp"

namespace someIp
{
    namespace sd
    {

        class LoadBalancingOption : public Option
        {
        private:
            uint8_t m_reserved;
            /* [PRS_SOMEIPSD_00542] */
            uint16_t m_priority;
            uint16_t m_weight;

        public:
            LoadBalancingOption();
            LoadBalancingOption(uint16_t priority, uint16_t weight, bool discardable);

            uint16_t get_priority() const { return m_priority; };

            uint16_t get_weight() const { return m_weight; };

            void serialize(Serializer &ref_serializer) const;
            bool deserialize(Deserializer &ref_deserializer);
        };

    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_LOAD_BALANCING_OPTION_HPP_
