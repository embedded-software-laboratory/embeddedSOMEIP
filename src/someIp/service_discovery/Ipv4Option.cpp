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

#include "Ipv4Option.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char SD_OPTION_IPV4_TAG[] = "SD_OPTION_IPV4";
using SD_IPV4_LOGGER = BaseLogger<SD_OPTION_IPV4_TAG>;

namespace someIp
{
    namespace sd
    {
        Ipv4Option::Ipv4Option(bool is_multicast) : IpOption(is_multicast)
        {
            m_length = 0x0009;
        }

        Ipv4Option::Ipv4Option(IpAddr ip_address,
                                 uint16_t port,
                                 SdTransportProtocol protocol,
                                 bool is_multicast)
            : IpOption(ip_address,
                        port,
                        protocol,
                        is_multicast)
        {

            m_length = 0x0009;
            SD_IPV4_LOGGER::debug("IPv4 option created with type: ");
            print_type();
            SD_IPV4_LOGGER::debug("--------------------------------------------------------------");
        }

        void Ipv4Option::serialize(Serializer &ref_serializer) const
        {
            Option::serialize(ref_serializer);
            // IpAddr is host byte order, wire is big-endian
            ref_serializer.serialize(m_ip.to_u32());

            /* reserved byte */
            ref_serializer.serialize(static_cast<uint8_t>(0x00));
            ref_serializer.serialize(m_transport_protocol);
            ref_serializer.serialize(m_port);
        }

        bool Ipv4Option::deserialize(Deserializer &ref_deserializer)
        {
            bool successful = true;
            successful = Option::deserialize(ref_deserializer);
            uint32_t temp;
            // deserializer yields host byte order
            successful = successful && ref_deserializer.deserialize(temp);
            m_ip = IpAddr::from_u32(temp);
            successful = successful && ref_deserializer.dump(1);
            successful = successful && ref_deserializer.deserialize(m_transport_protocol);
            successful = successful && ref_deserializer.deserialize(m_port);
            SD_IPV4_LOGGER::debug("IPv4 Option Deserialized: ");
            print_type();
            print_str_ip();
            SD_IPV4_LOGGER::debug("Port: %d", static_cast<int>(m_port));
            SD_IPV4_LOGGER::debug("Transport Protocol: %d", static_cast<int>(m_transport_protocol));
            successful ? SD_IPV4_LOGGER::debug("IPv4 Option Deserialized Successfully!")
                       : SD_IPV4_LOGGER::debug("Failed to Deserialize IPv4 Option!");
            SD_IPV4_LOGGER::debug("----------------------------------------------------------");

            return successful;
        }
    } // namespace sd
} // namespace someIp
