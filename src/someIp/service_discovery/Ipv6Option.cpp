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

#include "Ipv6Option.hpp"
#include <cstring>

namespace someIp
{
    namespace sd
    {
        Ipv6Option::Ipv6Option(bool is_multicast) : IpOption(is_multicast)
        {
            m_length = 0x0015;
        }

        Ipv6Option::Ipv6Option(const IpAddr &ref_ipv6_address,
                                 uint16_t port,
                                 SdTransportProtocol protocol,
                                 bool is_multicast)
            : IpOption(ref_ipv6_address, port, protocol, is_multicast)
        {
            m_length = 0x0015;
#if LWIP_IPV6
            const ip6_addr_t *ipv6 = ip_2_ip6(&ipv6_address);
            std::memcpy(_ipv6.data(), ipv6->addr, 16);
#endif
        }

        void Ipv6Option::serialize(Serializer &ref_serializer) const
        {
            Option::serialize(ref_serializer);
            ref_serializer.serialize(m_ipv6);
            /* reserved byte */
            ref_serializer.serialize(static_cast<uint8_t>(0));
            ref_serializer.serialize(static_cast<uint8_t>(m_transport_protocol));
            ref_serializer.serialize(m_port);
        }

        bool Ipv6Option::deserialize(Deserializer &ref_deserializer)
        {
            bool successful;
            successful = Option::deserialize(ref_deserializer);
            successful = successful && ref_deserializer.deserialize(m_ipv6);
            successful = successful && ref_deserializer.dump(static_cast<uint8_t>(0));
            successful = successful && ref_deserializer.deserialize(m_transport_protocol);
            successful = successful && ref_deserializer.deserialize(m_port);
            return successful;
        }
    } // namespace sd
} // namespace someIp