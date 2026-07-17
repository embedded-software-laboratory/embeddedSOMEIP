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

#include "IpOption.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char SD_OPTION_IPV4_TAG[] = "SD_OPTION_IPV4";
using SD_IPV4_LOGGER = BaseLogger<SD_OPTION_IPV4_TAG>;

namespace someIp
{
    namespace sd
    {
        IpOption::IpOption(bool is_multicast) : Option(is_multicast ? SdOptionType::IPV4_MULTICAST : SdOptionType::IPV4_ENDPOINT, false),
                                                  m_ip(IpAddr::any()),
                                                  m_port(0),
                                                  m_transport_protocol(static_cast<uint8_t>(SdTransportProtocol::UDP)) {}

        IpOption::IpOption(IpAddr ip_address,
                             uint16_t port,
                             SdTransportProtocol protocol,
                             bool is_multicast)
            : Option(is_multicast ? SdOptionType::IPV4_MULTICAST : SdOptionType::IPV4_ENDPOINT, false),
              m_ip(ip_address),
              m_port(port),
              m_transport_protocol(static_cast<uint8_t>(protocol)) {}

        void IpOption::print_str_ip() const
        {
            SD_IPV4_LOGGER::debug("IP Address: ");
            SD_IPV4_LOGGER::debug(m_ip.c_str());
        }

    } // namespace sd
} // namespace someIp
