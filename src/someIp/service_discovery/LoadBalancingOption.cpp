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

#include "LoadBalancingOption.hpp"
#include <cstdio>
#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char SD_LOAD_BALANCING_OPTION[] = "LOAD_BALANCING";
using LOAD_BALANCING_LOGGER = BaseLogger<SD_LOAD_BALANCING_OPTION>;

namespace someIp
{
    namespace sd
    {

        LoadBalancingOption::LoadBalancingOption()
            : Option(SdOptionType::LOAD_BALANCING), m_reserved(0), m_priority(0), m_weight(0)
        {
            m_length = 0x0005;
        }

        LoadBalancingOption::LoadBalancingOption(uint16_t priority, uint16_t weight, bool discardable)
            : Option(SdOptionType::LOAD_BALANCING, discardable), m_priority(priority), m_weight(weight)
        {
            m_length = 0x0005;
        }

        void LoadBalancingOption::serialize(Serializer &ref_serializer) const
        {
            Option::serialize(ref_serializer);
            ref_serializer.serialize(m_reserved);
            ref_serializer.serialize(m_priority);
            ref_serializer.serialize(m_weight);
        }

        bool LoadBalancingOption::deserialize(Deserializer &ref_deserializer)
        {
            bool successful = true;
            successful = Option::deserialize(ref_deserializer);
            successful = successful && ref_deserializer.deserialize(m_reserved);
            successful = successful && ref_deserializer.deserialize(m_priority);
            successful = successful && ref_deserializer.deserialize(m_weight);
            LOAD_BALANCING_LOGGER::debug("Load Balancing Option Deserialized:\n");
            LOAD_BALANCING_LOGGER::debug("Priority: ", m_priority);
            LOAD_BALANCING_LOGGER::debug("Weight: ", m_weight);
            LOAD_BALANCING_LOGGER::debug("Discardable flag: ", static_cast<int>(m_discardable_flag >> 7));
            successful ? LOAD_BALANCING_LOGGER::debug("Load Balancing Option Deserialized Successfully!")
                       : LOAD_BALANCING_LOGGER::debug("Failed to Deserialize Load Balancing Option!");
            LOAD_BALANCING_LOGGER::debug("---------------------------------------------------------");
            return successful;
        }

    } // namespace sd
} // namespace someIp
