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

#ifndef SOMEIP_UDP_DRIVER_HPP
#define SOMEIP_UDP_DRIVER_HPP

#include "someIp/os/Os.hpp"
#include <array>
#include <memory>

#include "UdpConnection.hpp"
#include "IUdpDriver.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/config/config.hpp"
#include "someIp/pattern/Singleton.hpp"
#include "someIp/structs/PbufWrapper.hpp"
#include "someIp/communication/ITransport.hpp"

namespace someIp
{
  // manages all UDP via lwIP, keeps locking with the lwIP core thread

  struct UdpParam
  {
    os::Semaphore initSem;
    ip_addr_t ipaddr;
  };

  struct Context
  {
    ip_addr_t multicast_ip;
    ip_addr_t local_ip;
  };

  class UdpDriver : public IUdpDriver, Singleton<UdpDriver>
  {
  public:
    typedef void (*udpRxPackage_fp)(void *ptr_arg, PackageRx &&ref_rx);

    void join_multicast_group(ip_addr_t multicast_ip);

    static UdpDriver* get_instance()
    {
      return Singleton<UdpDriver>::get_instance();
    }

    // /**
    //  * Sets the callback for Service Discovery packets
    //  *
    //  * @param sdCallback is called when a new packet for SD is received
    //  * @param sdArgsCallback args for the callback
    //  */
    // void set_sd_callback(udpRxPackage_fp sdCallback, void *sdArgsCallback)
    // {
    //   this->sdRxCallback = sdCallback;
    //   this->sdArgsCallback = sdArgsCallback;
    // }

    void init(udpRxPackage_fp ptr_callback, void *ptr_argsCallback, ip_addr_t &ref_localIp);

    // rx sink for raw datagrams from lwIP
    static void set_rx_sink(RxCallback cb);

    virtual bool send_udp_packet(
        ip_addr_t &ref_destAddr,
        uint16_t destPort,
        PbufWrapper &&ref_buffer,
        uint16_t src_port);

    virtual bool send_udp_packet(
        ip_addr_t &ref_destAddr,
        uint16_t destPort,
        PbufWrapper &&ref_buffer);

    virtual UdpConnection *create_udp_connection(uint16_t receivePort);

  protected:
    UdpDriver();

  private:
    std::size_t m_numberOfConnections = 0;
    static std::array<UdpConnection, config::SOMEIP_MAX_NUM_CONNECTIONS> m_connections;
    someIp::os::Mutex m_table_mtx; // guards the connection table (M-20)

    udpRxPackage_fp m_ptr_rxCallback;
    void *m_ptr_argsCallback;
    // udpRxPackage_fp sdRxCallback;
    // void *sdArgsCallback;

    ip_addr_t m_local_ip;

    friend void udpRxWrapper(void *ptr_arg, udp_pcb *ptr_pcb, pbuf *ptr_p, const ip_addr_t *ptr_addr, uint16_t port);

    friend class Singleton<UdpDriver>;
  };

} // namespace someIp

#endif // SOMEIP_UDP_DRIVER_HPP
