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

#include "Option.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char SD_OPTION_TAG[] = "OPTION";
using SD_OPTION_LOGGER = BaseLogger<SD_OPTION_TAG>;

namespace someIp
{
    namespace sd
    {

        Option::Option(SdOptionType type, bool discardable) : m_type(type), m_discardable_flag(static_cast<uint8_t>(discardable)) {}

        void Option::serialize(Serializer &ref_serializer) const
        {
            ref_serializer.serialize(m_length);
            ref_serializer.serialize(static_cast<uint8_t>(m_type));
            ref_serializer.serialize(static_cast<uint8_t>(m_discardable_flag << 7));
        }

        bool Option::deserialize(Deserializer &ref_deserializer)
        {
            bool successful = true;
            /* get option length*/
            successful = successful && ref_deserializer.deserialize(m_length);

            /* get option type */
            uint8_t tmp;
            successful = successful && ref_deserializer.deserialize(tmp);
            m_type = static_cast<SdOptionType>(tmp);

            /* get option discardable flag */
            successful = successful && ref_deserializer.deserialize(m_discardable_flag);

            SD_OPTION_LOGGER::debug("---------------------------------------------------------------------------");
            SD_OPTION_LOGGER::debug("Option Length: ", m_length);
            SD_OPTION_LOGGER::debug("Option Type: ");
            Option::print_type();
            return successful;
        }

        void Option::print_type() const
        {
            switch (m_type)
            {
            case SdOptionType::CONFIGURATION:
                SD_OPTION_LOGGER::debug("Configuration Option ");
                break;
            case SdOptionType::LOAD_BALANCING:
                SD_OPTION_LOGGER::debug("Load Balancing Option ");
                break;
            case SdOptionType::IPV4_ENDPOINT:
                SD_OPTION_LOGGER::debug("IPv4 Endpoint Option ");
                break;
            case SdOptionType::IPV6_ENDPOINT:
                SD_OPTION_LOGGER::debug("IPv6 Endpoint Option ");
                break;
            case SdOptionType::IPV4_MULTICAST:
                SD_OPTION_LOGGER::debug("IPv4 Multicast Option ");
                break;
            case SdOptionType::IPV6_MULTICAST:
                SD_OPTION_LOGGER::debug("IPv6 Multicast Option ");
                break;
            default:
                SD_OPTION_LOGGER::debug("Unknown Option Type ");
                break;
            }
        }

    } // namespace sd
} // namespace someIp
