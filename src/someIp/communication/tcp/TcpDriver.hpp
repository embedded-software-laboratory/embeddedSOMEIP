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

#ifndef SOMEIP_TCP_DRIVER_HPP
#define SOMEIP_TCP_DRIVER_HPP

#include <array>
#include <memory>
#include <lwip/tcp.h>
#include <lwip/ip_addr.h>

#include "TcpConnection.hpp"
#include "ITcpDriver.hpp"
#include "someIp/config/config.hpp"
#include "someIp/pattern/Singleton.hpp"
#include "someIp/structs/PbufWrapper.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/communication/ITransport.hpp"


namespace someIp { 

// manages all TCP via lwIP, avoids races with the lwIP core thread
class TcpDriver: public ITcpDriver,  Singleton<TcpDriver>
{
public:
    typedef void (*tcpRxPackage_fp)(void* ptr_arg, PackageRx &&ref_rx);
    
    static TcpDriver* get_instance() {
      return Singleton<TcpDriver>::get_instance();
    }

    someIp::TcpConnection *create_client_tcp_connection(const ip_addr_t& ref_remote_ip, uint16_t remote_port, void* ptr_user_arg);

    void reset();

    bool is_connected(const ip_addr_t remote_ip, uint16_t remote_port);

    TcpConnection* find_tcp_connection(const ip_addr_t& ref_destAddr, uint16_t port);

    TcpConnection* find_tcp_connection_by_pcb(struct tcp_pcb* ptr_pcb);

    void init(tcpRxPackage_fp ptr_callback, void *ptr_argsCallback, ip_addr_t &ref_localIp);

    // rx sink for reassembled SOME/IP messages
    static void set_rx_sink(RxCallback cb);

    virtual TcpConnection *create_server_tcp_connection(uint16_t receivePort);


    virtual bool send_tcp_packet(
      ip4_addr_t &ref_destAddr, 
      uint16_t destPort, 
      PbufWrapper &&ref_buffer
    );


    virtual bool send_tcp_packet_pcb(
      TcpConnection* ptr_tcpConnection, 
      PbufWrapper &&ref_buffer
    );

    void* get_callback(){ return &argsCallback; }

    TcpDriver();


  std::size_t numberOfConnections = 0;
  static std::array<TcpConnection, config::SOMEIP_MAX_NUM_CONNECTIONS> connections;

  tcpRxPackage_fp rxCallback;
  void* argsCallback;

  friend err_t tcpAcceptWrapper(void *ptr_arg, tcp_pcb *ptr_pcb, pbuf *ptr_p, const ip_addr_t *ptr_addr, uint16_t port);

  friend err_t tcpRxWrapper(void * ptr_arg, struct tcp_pcb *ptr_tcpb, struct pbuf *ptr_p, err_t err);

  friend err_t tcpConnectedWrapper(void *ptr_arg, struct tcp_pcb *ptr_tpcb, err_t err);

  void print_connections();

  friend class Singleton<TcpDriver>;
};
}

#endif //SOMEIP_TCP_DRIVER_HPP
