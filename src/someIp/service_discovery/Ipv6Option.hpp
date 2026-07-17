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

// #if LWIP_IPV6

#ifndef SOMEIP_SD_IPV6_OPTION_HPP_
#define SOMEIP_SD_IPV6_OPTION_HPP_

#include <array>

#include "IpOption.hpp"
#ifdef SOMEIP_PLATFORM_STM32
#include "lwip/ip6_addr.h"
#endif

namespace someIp
{
    namespace sd
    {
        class Ipv6Option : public IpOption
        {
        private:
            std::array<uint8_t, 16> m_ipv6;
            // void to_ipv6_bytes(const ip6_addr_t *ipv6_address);

        public:
            Ipv6Option(bool is_multicast = false);
            Ipv6Option(const IpAddr &ref_ipv6_address,
                        uint16_t port,
                        SdTransportProtocol transport_protocol_port = SdTransportProtocol::UDP,
                        bool is_multicast = false);

            void serialize(Serializer &ref_serializer) const;
            bool deserialize(Deserializer &ref_deserializer);
        };
    } // namespace sd
} // namespace someIP

#endif // SOMEIP_SD_IPV6_OPTION_HPP_
// #endif // LWIP_IPV6
