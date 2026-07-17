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

#ifndef SOMEIP_SD_IP_OPTION_HPP_
#define SOMEIP_SD_IP_OPTION_HPP_

#include "Option.hpp"
#include "someIp/net/IpAddress.hpp"

namespace someIp
{
    namespace sd
    {
        class IpOption : public Option
        {
        protected:
            IpAddr m_ip;
            uint16_t m_port;
            uint8_t m_transport_protocol;

        public:
            IpOption(bool is_multicast = false);
            IpOption(IpAddr ip_address,
                      uint16_t port,
                      SdTransportProtocol protocol = SdTransportProtocol::UDP,
                      bool is_multicast = false);
            virtual ~IpOption() {};

            IpAddr get_ip_address() const { return m_ip; };
            void set_ip_address(IpAddr ip_address) { m_ip = ip_address; };

            uint16_t get_port() const { return m_port; };
            void set_port(uint16_t port) { m_port = port; };

            bool is_tcp() const { return m_transport_protocol == static_cast<uint8_t>(SdTransportProtocol::TCP); }

            void print_str_ip() const;

            SdEndpointInfo get_endpoint() const { return SdEndpointInfo{m_ip, m_port}; }
        };
    } // namespace sd
} // namespace someIP

#endif // SOMEIP_SD_IP_OPTION_HPP_
