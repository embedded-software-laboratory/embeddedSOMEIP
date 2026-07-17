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

#include "ConfigurationOption.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char CONFIG_OPTION_TAG[] = "CONFIGURATION_OPTION";
using CONFIG_OPTION_LOGGER = BaseLogger<CONFIG_OPTION_TAG>;

namespace someIp
{
    namespace sd
    {
        ConfigurationOption::ConfigurationOption(bool discardable) : Option(SdOptionType::CONFIGURATION, discardable)
        {
            // discardable flag byte + trailing 0x00 length byte [PRS_SOMEIPSD_00280]
            m_length = 2;
        }

        void ConfigurationOption::serialize(Serializer &ref_serializer) const
        {
            Option::serialize(ref_serializer);
            // raw config string (length-prefixed entries + trailing 0x00)
            for (uint8_t b : m_config_bytes)
            {
                ref_serializer.serialize(b);
            }
            // trailing 0x00 when nothing was stored
            if (m_config_bytes.empty())
            {
                ref_serializer.serialize(static_cast<uint8_t>(0x00));
            }
        }

        bool ConfigurationOption::deserialize(Deserializer &ref_deserializer)
        {
            if (!Option::deserialize(ref_deserializer))
            {
                CONFIG_OPTION_LOGGER::error("Failed to deserialize configuration option");
                return false;
            }

            // subtract the discardable flag byte already deserialized
            uint16_t n = (m_length > 1) ? static_cast<uint16_t>(m_length - 1) : 0;
            m_config_bytes.clear();
            uint8_t last = 0x00;
            for (uint16_t i = 0; i < n; ++i)
            {
                uint8_t b = 0;
                if (!ref_deserializer.deserialize(b))
                {
                    CONFIG_OPTION_LOGGER::error("Config string size mismatch");
                    return false;
                }
                last = b;
                if (i < config::MAX_CONFIG_OPTION_BYTES)
                {
                    m_config_bytes.push_back(b);
                }
            }

            if (n == 0)
            {
                CONFIG_OPTION_LOGGER::debug("config string can't be empty it should at least contain a 0x00 byte");
                return false;
            }

            // config string ends with a 0x00 length byte [PRS_SOMEIPSD_00280]
            if (last != 0x00)
            {
                CONFIG_OPTION_LOGGER::error("config string deserialization error");
                return false;
            }

            return true;
        }

    } // namespace sd
} // namespace someIp
