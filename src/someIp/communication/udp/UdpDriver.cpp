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
#include "lwip/def.h"

#include <lwip/igmp.h>
#include <lwip/tcpip.h>
#include <lwip/sys.h>
#include "lwip/tcp.h"
#include <lwip/ip_addr.h>

#ifndef SOMEIP_PLATFORM_STM32
// host-only lwIP config and netif backends
#include "lwipcfg.h"
#include "netif/tapif.h"
#include <netif/vdeif.h>
#endif

#include "UdpDriver.hpp"
#include "someIp/communication/TcpipCoreLock.hpp"
#include "someIp/config/logging_config.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/structs/PbufWrapper.hpp"
#include "someIp/communication/EndpointLwip.hpp"
#include "someIp/communication/PbufByteBuffer.hpp"

#include "someIp/communication/NetifShared.hpp"
#include "someIp/utils/StaticPool.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"
#include <cstring>
#include <sstream>

#include "someip_tp.h"

constexpr char UDP_DRIVER_TAG[] = "UDP_DRIVER";
using UDP_DRIVER_LOGGER = BaseLogger<UDP_DRIVER_TAG>;

namespace someIp{

std::array<UdpConnection, config::SOMEIP_MAX_NUM_CONNECTIONS> UdpDriver::m_connections = {};

// static struct netif netif;

// transport-neutral rx sink, set by LwipUdpTransport
static RxCallback g_udp_rx_sink;

void UdpDriver::set_rx_sink(RxCallback cb) { g_udp_rx_sink = std::move(cb); }

// deliver the raw datagram, SOME/IP parsing happens in eSomeIP::on_rx
void udpRxWrapper(void *ptr_arg, udp_pcb *ptr_pcb, pbuf *ptr_p, const ip_addr_t *ptr_addr, uint16_t port) {
  LWIP_UNUSED_ARG(ptr_arg);
  LWIP_UNUSED_ARG(ptr_pcb);
  if (ptr_p == nullptr) {
    return;
  }
  if constexpr (someIp::config::IS_IN_EVALUATION_MODE)
  {
    uint16_t sessionId = read_u16_be(ptr_p->payload, 10);
    lttng_ust_tracepoint(someip, lwip_udp_receive, ip4addr_ntoa(ptr_addr), port, sessionId);
  }
  UDP_DRIVER_LOGGER::log("Recieved packet from: %s:%d with size: %d", ip4addr_ntoa(ptr_addr), port, ptr_p->tot_len);

  if (!g_udp_rx_sink) {
    UDP_DRIVER_LOGGER::error("No RX sink registered - dropping packet");
    pbuf_free(ptr_p);
    return;
  }

  RxPacket rx;
  rx.source = endpoint_from_lwip(*ptr_addr, port);
  rx.buffer = make_pbuf_buffer(PbufWrapper(ptr_p));
  g_udp_rx_sink(std::move(rx));
}

#ifndef SOMEIP_PLATFORM_STM32
// host-only, bare metal already has a netif
static void tcpip_init_init(void *ptr_arg)
{
  if (ptr_arg == nullptr)
  {
    UDP_DRIVER_LOGGER::log("Failed to init. nullptr passed");
    return;
  }
  UdpParam *ptr_params = static_cast<UdpParam *>(ptr_arg);

  srand((unsigned int)time(nullptr));

  // ip4_addr_t ipaddr;
  ip4_addr_t netmask;
  ip4_addr_t gw;

  LWIP_PORT_INIT_GW(&gw);
  // LWIP_PORT_INIT_IPADDR(&ipaddr);
  LWIP_PORT_INIT_NETMASK(&netmask);

  // open question: sd may need (void *)"/tmp/vde.ctl" instead of nullptr
  netif_add(&netif, &ptr_params->ipaddr, &netmask, &gw, nullptr, tapif_init, tcpip_input); // problems on second call
  UDP_DRIVER_LOGGER::log("Starting lwIP, local interface IP is %s", ip4addr_ntoa(&ptr_params->ipaddr));
  netif_set_default(&netif);
  netif_set_link_up(&netif);
  netif_set_up(&netif);

  ptr_params->initSem.signal();
  UDP_DRIVER_LOGGER::log("tcpip_init_init finished");
}
#endif // !SOMEIP_PLATFORM_STM32

UdpDriver::UdpDriver()
{
  UDP_DRIVER_LOGGER::log("UdpDriver created");
}

// init the driver and lwIP, the semaphore waits for the tcpip thread
void UdpDriver::init(udpRxPackage_fp ptr_callback, void *ptr_argsCallback, ip_addr_t &ref_localIp)
{
  this->m_ptr_rxCallback = ptr_callback;
  this->m_ptr_argsCallback = ptr_argsCallback;
  this->m_local_ip = ref_localIp; // was uninitialized -> IGMP joins used 0.0.0.0 (M-20)
  UDP_DRIVER_LOGGER::log("LOCAL IP: %s", ipaddr_ntoa(&ref_localIp));

#ifndef SOMEIP_PLATFORM_STM32
  // host build brings up lwIP and a netif itself
  // no stdio buffering
  setvbuf(stdout, nullptr, _IONBF, 0);

  UdpParam params;            // initSem is an os::Semaphore
  params.ipaddr = ref_localIp;

  tcpip_init(tcpip_init_init, &params);

  params.initSem.wait();       // wait for the tcpip thread to finish init
  UDP_DRIVER_LOGGER::log("lwIP initialized");
#else
  // bare metal already has lwIP and gnetif up, caller passes the address
  UDP_DRIVER_LOGGER::log("udpdriver initialized (using existing netif)");
#endif
}

someIp::UdpConnection *UdpDriver::create_udp_connection(uint16_t receivePort)
{
  // serialize table access, called per-send from app threads (M-20)
  os::Guard table_lock(m_table_mtx);

  for (uint8_t i = 0; i < m_numberOfConnections; ++i)
  {
    if (m_connections[i].port == receivePort)
    {
      UDP_DRIVER_LOGGER::debug("Connection already exists");
      return &m_connections[i];
    }
  }

  if (m_numberOfConnections == m_connections.size()){
    UDP_DRIVER_LOGGER::error("Maximum number of connections reached");
    return nullptr;
  }

  UdpConnection udpConnection(receivePort); // new unbound pcb
  if (!udpConnection.pcb) { // udp_new pool exhausted (M-20)
    UDP_DRIVER_LOGGER::error("udp_new failed (pcb pool exhausted)");
    return nullptr;
  }

  UDP_DRIVER_LOGGER::log("Creating new connection");
  {
    TcpipCoreLock lock;
    err_t err;
#ifdef SOMEIP_PLATFORM_STM32
    // bind to ANY so it does not depend on the local address
    err = udp_bind(udpConnection.pcb, IP_ADDR_ANY, receivePort);
#else
    if (receivePort == config::SD_PORT)
      err = udp_bind(
          udpConnection.pcb,
          IP_ADDR_ANY,
          receivePort); // To receive multicast
    else
      err = udp_bind(
          udpConnection.pcb,
          &m_local_ip,
          receivePort); // To receive multicast
#endif
    if (err != ERR_OK && err != ERR_USE){
      UDP_DRIVER_LOGGER::error("Error binding udp pcb");
      return nullptr;
    }
    udp_recv(udpConnection.pcb, udpRxWrapper, this->m_ptr_argsCallback); // rx callback for this pcb
  }

  m_connections[m_numberOfConnections] = std::move(udpConnection);
  m_numberOfConnections++;

  return &m_connections[m_numberOfConnections - 1];
}

bool someIp::UdpDriver::send_udp_packet(
    ip_addr_t &ref_destAddr,
    uint16_t destPort,
    PbufWrapper &&ref_buffer,
    uint16_t src_port)
{
  err_t err;
  someIp::UdpConnection *ptr_udpConnection = create_udp_connection(src_port);
  if (!ptr_udpConnection || !ptr_udpConnection->pcb) { // pool/bind failure (M-20)
    UDP_DRIVER_LOGGER::error("send_udp_packet: no udp connection for src port %u", src_port);
    return true; // true == error here
  }
  {
    TcpipCoreLock lock;
    UDP_DRIVER_LOGGER::log("***************************************************************");
    UDP_DRIVER_LOGGER::log("sending udp packet from : %s:%d", ip4addr_ntoa(&ptr_udpConnection->pcb->local_ip), ptr_udpConnection->pcb->local_port);
    UDP_DRIVER_LOGGER::log("Sending udp packet to %s:%d", ip4addr_ntoa(&ref_destAddr), destPort);
    UDP_DRIVER_LOGGER::log("***************************************************************");
    // std::cout << "\n\nBUFFER LENGTH IS: " << buffer.get()->len << std::endl;
    err = udp_sendto(ptr_udpConnection->pcb, ref_buffer.get(), &ref_destAddr, destPort);
    if constexpr (someIp::config::DEBUG_LOG_ENABLED)
    {
      if (err != ERR_OK)
      {
        UDP_DRIVER_LOGGER::error("udp_sendto returned %s", lwip_strerr(err));
      }
    }
  }

  UDP_DRIVER_LOGGER::trace("lwIP udp_sendto returned %d", err);
  return err != ERR_OK;
}

bool someIp::UdpDriver::send_udp_packet(
    ip_addr_t &ref_destAddr,
    uint16_t destPort,
    PbufWrapper &&ref_buffer)
{
  err_t err;
  someIp::UdpConnection *ptr_udpConnection = create_udp_connection(config::SD_PORT);
  if (!ptr_udpConnection || !ptr_udpConnection->pcb) { // pool/bind failure (M-20)
    UDP_DRIVER_LOGGER::error("send_udp_packet: no udp connection for SD_PORT");
    return true; // true == error here
  }
  {
    TcpipCoreLock lock;
    UDP_DRIVER_LOGGER::log("Sending udp packet to %s:%d", ip4addr_ntoa(&ref_destAddr), destPort);
    
    /*
    printf("\tSending package with payload: ");
    for(int i = 0; i < buffer.get()->len; i++){
      printf("%02X ", ((uint8_t *)buffer.get()->payload)[i]);
    }
    printf("\n");
    */
    if constexpr (someIp::config::IS_IN_EVALUATION_MODE)
    {
      uint16_t sessionId = read_u16_be(ref_buffer.get()->payload, 10);
      lttng_ust_tracepoint(someip, lwip_udp_send, ip4addr_ntoa(&ref_destAddr), destPort, sessionId);
    }
    UDP_DRIVER_LOGGER::log("sending package of size: %d", ref_buffer.get()->tot_len);
    err = udp_sendto(ptr_udpConnection->pcb, ref_buffer.get(), &ref_destAddr, destPort);
    if constexpr (someIp::config::DEBUG_LOG_ENABLED){
      if (err != ERR_OK) {
        UDP_DRIVER_LOGGER::error("udp_sendto returned %s", lwip_strerr(err));
      }
    }
  }

  UDP_DRIVER_LOGGER::trace("lwIP udp_sendto returned %d", err);
  UDP_DRIVER_LOGGER::debug("udp_sendto returned with: code %d, (%s)", (int)err, lwip_strerr(err));
  return err != ERR_OK;
}

// fixed no-heap pool of contexts for the tcpip-thread join callback
static someIp::StaticPool<someIp::Context, 8> &join_ctx_pool()
{
  static someIp::StaticPool<someIp::Context, 8> pool;
  return pool;
}

void UdpDriver::join_multicast_group(ip_addr_t multicast_ip)
{
  Context *ptr_ctx = join_ctx_pool().acquire();
  if (!ptr_ctx) {
    UDP_DRIVER_LOGGER::error("multicast join context pool exhausted");
    return;
  }
  ptr_ctx->multicast_ip = multicast_ip;
  ptr_ctx->local_ip = this->m_local_ip;
  tcpip_try_callback([](void *ptr_arg){
      auto *ptr_c = static_cast<Context *>(ptr_arg);
      igmp_joingroup(ip_2_ip4(&ptr_c->local_ip), ip_2_ip4(&ptr_c->multicast_ip));
      join_ctx_pool().release(ptr_c);
  }, ptr_ctx);
}

} // namespace someIp
