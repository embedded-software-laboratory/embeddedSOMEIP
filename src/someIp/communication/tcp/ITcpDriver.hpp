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

#ifndef SOMEIP_ITCP_DRIVER_HPP
#define SOMEIP_ITCP_DRIVER_HPP

#include <memory>
#include <lwip/ip4_addr.h>
#include "TcpConnection.hpp"
#include "someIp/structs/PbufWrapper.hpp"


namespace someIp {

class ITcpDriver {
  public:
    virtual ~ITcpDriver() = default;
    virtual bool send_tcp_packet(ip4_addr_t &ref_destAddr, uint16_t destPort, PbufWrapper &&ref_buffer) = 0;
    virtual bool send_tcp_packet_pcb(TcpConnection* ptr_tcpConnection, PbufWrapper &&ref_buffer) = 0;
    virtual void reset() = 0;
    virtual TcpConnection* create_server_tcp_connection(uint16_t receivePort) = 0;
    virtual someIp::TcpConnection *create_client_tcp_connection(const ip_addr_t& ref_remote_ip, uint16_t remote_port, void* ptr_user_arg) = 0;
     
};

} // namespace someIp

#endif // SOMEIP_IUDP_DRIVER_HPP