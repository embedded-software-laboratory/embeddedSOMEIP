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

#ifndef SOMEIP_PLATFORM_STM32
// host-only lwIP config and netif backends
#include "lwipcfg.h"
#include "netif/tapif.h"
#include <netif/vdeif.h>
#endif

#include "TcpDriver.hpp"
#include "someIp/communication/TcpipCoreLock.hpp"
#include "someIp/config/logging_config.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/structs/PbufWrapper.hpp"
#include "someIp/communication/EndpointLwip.hpp"
#include "someIp/communication/PbufByteBuffer.hpp"
#include "lwip/stats.h"

#include "someIp/communication/NetifShared.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"
#include "someIp/utils/WireBounds.hpp"
#include "someIp/communication/tcp/TcpStream.hpp"

#include "someip_tp.h"

constexpr char TCP_DRIVER_TAG[] = "TCP_DRIVER";
using TCP_DRIVER_LOGGER = BaseLogger<TCP_DRIVER_TAG>;

namespace someIp
{

std::array<TcpConnection, config::SOMEIP_MAX_NUM_CONNECTIONS> TcpDriver::connections = {};

// transport-neutral rx sink, set by LwipTcpTransport
static RxCallback g_tcp_rx_sink;

void TcpDriver::set_rx_sink(RxCallback cb) { g_tcp_rx_sink = std::move(cb); }


TcpConnection* TcpDriver::find_tcp_connection(const ip_addr_t& ref_destAddr, uint16_t port)
{
  for(uint8_t i = 0; i < config::SOMEIP_MAX_NUM_CONNECTIONS; ++i)
  {
    if(connections[i].pcb &&
       connections[i].pcb->remote_port == port &&
       ip_addr_cmp(&connections[i].pcb->remote_ip, &ref_destAddr))
    {
      return &connections[i]; 
    }
  }
  return nullptr; 
}


TcpConnection* find_tcp_connection_by_pcb(struct tcp_pcb* ptr_pcb)
{
  for(auto& ref_conn : TcpDriver::connections)
  {
    if(ref_conn.pcb == ptr_pcb) return &ref_conn;
  }
  return nullptr;
}


// pointer into a pbuf chain at a byte offset
static void* ptr_at_offset(pbuf* ptr_p, u32_t offset, u16_t* ptr_avail_here)
{
  while (ptr_p && offset >= ptr_p->len) {
    offset -= ptr_p->len;
    ptr_p = ptr_p->next;
  }
  if (!ptr_p) { *ptr_avail_here = 0; return nullptr; }
  u32_t left = (u32_t)ptr_p->len - offset;
  *ptr_avail_here = (u16_t)LWIP_MIN(left, (u32_t)0xFFFF);
  return (uint8_t*)ptr_p->payload + offset;
}


// free fully acknowledged pbufs at the head of the tx queue
static void drain_acked_head(TcpConnection* ptr_c)
{
  if (!ptr_c->tx.p) return;

  u32_t ack_before = ptr_c->tx.acked;   // ack progress relative to current head
  pbuf* ptr_p = ptr_c->tx.p;
  u32_t left = ack_before;
  u32_t removed = 0;

  while (ptr_p && left >= ptr_p->len) {
    left    -= ptr_p->len;
    removed += ptr_p->len;
    pbuf* ptr_nxt = ptr_p->next;
    ptr_p->next = nullptr;     // detach before free
    pbuf_free(ptr_p);
    ptr_p = ptr_nxt;
  }

  ptr_c->tx.p     = ptr_p;
  ptr_c->tx.acked = left;      // leftover offset within new head

  // sent was an offset from the old head
  if (ptr_c->tx.sent >= removed) {
    ptr_c->tx.sent -= removed;
  } else {
    ptr_c->tx.sent = 0;
  }

  if (ptr_c->tx.sent < ptr_c->tx.acked)
    ptr_c->tx.sent = ptr_c->tx.acked;

  if (!ptr_c->tx.p) {
    // everything sent and acked
    ptr_c->tx.active = false;
    ptr_c->tx.sent = ptr_c->tx.acked = 0;
  }
}



// queue as much as sndbuf/sndqueuelen allow, zero-copy into lwIP
static bool pump_send(TcpConnection* ptr_c)
{
  struct tcp_pcb* ptr_pcb = ptr_c->pcb;
  if (!ptr_pcb || !ptr_c->tx.p) return true;

  drain_acked_head(ptr_c);
  if (!ptr_c->tx.p) return true;

  while (true) {
    u16_t snd_bytes_free = tcp_sndbuf(ptr_pcb);
    u16_t q_used  = tcp_sndqueuelen(ptr_pcb);
    u16_t q_limit = TCP_SND_QUEUELEN;
    if (snd_bytes_free == 0 || q_used >= q_limit) break;

    u32_t total_from_head = ptr_c->tx.p->tot_len;           // bytes remaining from head to end
    if (ptr_c->tx.sent >= total_from_head) break;           // nothing left to queue

    u16_t q_free = (u16_t)(q_limit - q_used);
    u32_t q_bytes_budget = (u32_t)q_free * (u32_t)TCP_MSS;

    u32_t remaining = total_from_head - ptr_c->tx.sent;
    u32_t to_write  = remaining;
    if (to_write > snd_bytes_free) to_write = snd_bytes_free;
    if (to_write > q_bytes_budget) to_write = q_bytes_budget;

    // keep each write modest for queue slots and segmenting
    const u32_t MAX_CHUNK = (u32_t)4 * (u32_t)TCP_MSS;
    if (to_write > MAX_CHUNK) to_write = MAX_CHUNK;
    if (to_write == 0) break;

    u16_t avail_here = 0;
    void* ptr_ptr = ptr_at_offset(ptr_c->tx.p, ptr_c->tx.sent, &avail_here);
    if (!ptr_ptr || avail_here == 0) break;

    u16_t this_write = (u16_t)LWIP_MIN((u32_t)avail_here, to_write);

    // COPY=0 so lwIP references the buffer instead of copying it
    err_t we = tcp_write(ptr_pcb, ptr_ptr, this_write, 0);
    if (we == ERR_MEM) {
      (void)tcp_output(ptr_pcb);  // try to free segments
      break;
    }
    if (we != ERR_OK) {
      TCP_DRIVER_LOGGER::error("pump_send: tcp_write failed: %s", lwip_strerr(we));
      return false;
    }

    ptr_c->tx.sent += this_write;

    // push now if the queue is getting full
    if (tcp_sndqueuelen(ptr_pcb) > (u16_t)((TCP_SND_QUEUELEN * 3) / 4)) {
      err_t oe = tcp_output(ptr_pcb);
      if (oe != ERR_OK) {
        TCP_DRIVER_LOGGER::error("pump_send: tcp_output failed: %s", lwip_strerr(oe));
        return false;
      }
    }
  }

  (void)tcp_output(ptr_pcb); // harmless if nothing new
  return true;
}




static err_t on_tcp_sent(void* ptr_arg, struct tcp_pcb* ptr_pcb, u16_t acked)
{
  LWIP_UNUSED_ARG(ptr_arg);
  TcpConnection* ptr_c = find_tcp_connection_by_pcb(ptr_pcb);
  if (!ptr_c) return ERR_OK;

  ptr_c->tx.acked += acked;
  drain_acked_head(ptr_c);
  (void)pump_send(ptr_c);
  return ERR_OK;
}







// skip tcp_close when the pcb was already freed (C-10)
static void clear_connection_slot(TcpConnection* ptr_c, bool pcb_already_freed) {
  if (!ptr_c) return;
  if (!pcb_already_freed && ptr_c->pcb) {
    tcp_close(ptr_c->pcb);
  }
  ptr_c->pcb = nullptr;
  if (ptr_c->tx.p) { pbuf_free(ptr_c->tx.p); ptr_c->tx.p = nullptr; }
  ptr_c->tx.active = false;
  ptr_c->tx.sent = ptr_c->tx.acked = 0;
  ptr_c->expectedLen = 0;
  ptr_c->receivedLen = 0;
}

// lwIP err callback, the pcb is already freed (C-10)
static void tcpErrWrapper(void* ptr_arg, err_t err) {
  TCP_DRIVER_LOGGER::error("tcp connection error: %s", lwip_strerr(err));
  clear_connection_slot(static_cast<TcpConnection*>(ptr_arg), /*pcb_already_freed=*/true);
}

// drops the message on pbuf OOM (C-9)
static void deliver_tcp_message(TcpConnection* ptr_conn, struct tcp_pcb* ptr_tpcb,
                                const uint8_t* ptr_data, size_t len) {
  if (!g_tcp_rx_sink) {
    TCP_DRIVER_LOGGER::error("No RX sink registered - dropping tcp message");
    return;
  }
  pbuf* ptr_msg = pbuf_alloc(PBUF_RAW, static_cast<u16_t>(len), PBUF_RAM);
  if (!ptr_msg) {
    TCP_DRIVER_LOGGER::error("OOM allocating reassembled tcp message; dropping");
    return;
  }
  pbuf_take(ptr_msg, ptr_data, static_cast<u16_t>(len));

  const uint16_t remote_port = ptr_tpcb->remote_port;
  const ip_addr_t remote_ip = ptr_tpcb->remote_ip;

  if constexpr (someIp::config::IS_IN_EVALUATION_MODE) {
    uint16_t sessionId = 0;
    if (len >= 12) sessionId = read_u16_be(ptr_msg->payload, 10);
    lttng_ust_tracepoint(someip, lwip_tcp_receive, ip4addr_ntoa(&remote_ip), remote_port, sessionId);
  }

  RxPacket rx;
  rx.source = endpoint_from_lwip(remote_ip, remote_port);
  rx.buffer = make_pbuf_buffer(PbufWrapper(ptr_msg));
  rx.conn = ConnectionId(TransportKind::TCP, ptr_conn);
  g_tcp_rx_sink(std::move(rx));
}

// frame SOME/IP messages by length across segment boundaries via conn->inbuf (C-13)
err_t tcpRxWrapper(void * ptr_arg, struct tcp_pcb* ptr_tpcb, struct pbuf *ptr_p, err_t err){
  LWIP_UNUSED_ARG(ptr_arg);
  LWIP_UNUSED_ARG(err);
  if(!ptr_p) return ERR_OK; // remote FIN, close is handled via tcp_err

  TcpConnection* ptr_conn = find_tcp_connection_by_pcb(ptr_tpcb);
  if(!ptr_conn){
    // unknown pcb, still consume the window and free the pbuf
    const u16_t n = ptr_p->tot_len;
    tcp_recved(ptr_tpcb, n);
    pbuf_free(ptr_p);
    return ERR_OK;
  }

  const u16_t seg_total = ptr_p->tot_len;
  u16_t in_off = 0; // read offset within the pbuf chain

  while (in_off < seg_total) {
    // learn the full message length once 8 header bytes are buffered
    if (ptr_conn->expectedLen == 0) {
      const size_t need = 8 - ptr_conn->receivedLen;
      const size_t take = LWIP_MIN(need, static_cast<size_t>(seg_total - in_off));
      pbuf_copy_partial(ptr_p, ptr_conn->inbuf.data() + ptr_conn->receivedLen, static_cast<u16_t>(take), in_off);
      ptr_conn->receivedLen += take;
      in_off = static_cast<u16_t>(in_off + take);
      if (ptr_conn->receivedLen < 8) break; // header not complete yet

      uint32_t length = 0;
      memcpy(&length, ptr_conn->inbuf.data() + 4, 4);
      length = ntohl(length);
      // validate before sizing, guards the +8 wrap and the buffer cap (C-2)
      if (!wire::valid_someip_length(length, config::MAX_TCP_RX_MESSAGE)) {
        TCP_DRIVER_LOGGER::error("invalid tcp message length %u; resetting connection", (unsigned)length);
        // stream desynced, drop the state and the pbuf
        ptr_conn->expectedLen = 0;
        ptr_conn->receivedLen = 0;
        tcp_recved(ptr_tpcb, seg_total);
        pbuf_free(ptr_p);
        return ERR_OK;
      }
      ptr_conn->expectedLen = static_cast<size_t>(length) + 8;
    }

    const size_t need = ptr_conn->expectedLen - ptr_conn->receivedLen;
    const size_t take = LWIP_MIN(need, static_cast<size_t>(seg_total - in_off));
    pbuf_copy_partial(ptr_p, ptr_conn->inbuf.data() + ptr_conn->receivedLen, static_cast<u16_t>(take), in_off);
    ptr_conn->receivedLen += take;
    in_off = static_cast<u16_t>(in_off + take);

    if (ptr_conn->receivedLen < ptr_conn->expectedLen) break; // need more segments

    // deliver and continue with the leftover bytes
    deliver_tcp_message(ptr_conn, ptr_tpcb, ptr_conn->inbuf.data(), ptr_conn->expectedLen);
    ptr_conn->expectedLen = 0;
    ptr_conn->receivedLen = 0;
  }

  tcp_recved(ptr_tpcb, seg_total); // consume the whole segment from the window
  pbuf_free(ptr_p);
  return ERR_OK;
}





err_t tcpConnectedWrapper(void *ptr_arg, struct tcp_pcb *ptr_tpcb, err_t err)
{
  if (err == ERR_OK){
    tcp_nagle_disable(ptr_tpcb); // avoid sawtooth in latency
    printf("[TcpDriver] TCP connection established (pcb=%p)\n", (void*)ptr_tpcb); 
    return ERR_OK; 
  } else {
    printf("[TcpDriver] TCP connection failed: %s\n", lwip_strerr(err)); 
    tcp_abort(ptr_tpcb); 
    return err; 
  }
}



err_t tcpAcceptWrapper(void *ptr_arg, struct tcp_pcb *ptr_newpcb, err_t err){
  auto* ptr_driver = static_cast<TcpDriver*>(ptr_arg);

  // honor accept error / null pcb instead of dereferencing blindly (M-13)
  if (err != ERR_OK || ptr_newpcb == nullptr || ptr_driver == nullptr) {
    TCP_DRIVER_LOGGER::error("tcp accept failed: %s", lwip_strerr(err));
    return ERR_VAL;
  }

  // abort the orphan pcb rather than leaking it (M-13/m-9)
  if (ptr_driver->numberOfConnections >= ptr_driver->connections.size()) {
    TCP_DRIVER_LOGGER::error("connection table full; aborting accepted pcb");
    tcp_abort(ptr_newpcb);
    return ERR_ABRT;
  }

  TCP_DRIVER_LOGGER::log("new tcp connection accepted %p\n", (void*)ptr_newpcb);

  // stable slot first so tcp_err can find it (C-10)
  size_t idx = ptr_driver->numberOfConnections++;
  TcpConnection conn;
  conn.pcb = ptr_newpcb;
  ptr_driver->connections[idx] = std::move(conn);
  TcpConnection* ptr_slot = &ptr_driver->connections[idx];

  tcp_nagle_disable(ptr_newpcb);
  tcp_arg(ptr_newpcb, ptr_slot);
  tcp_recv(ptr_newpcb, tcpRxWrapper);
  tcp_err(ptr_newpcb, tcpErrWrapper);

  // release the backlog slot or the listener bricks (M-13)
  tcp_backlog_accepted(ptr_newpcb);

  TCP_DRIVER_LOGGER::log("Accepted new TCP connection and saved in connections \n");
  return ERR_OK;
}





void TcpDriver::init(tcpRxPackage_fp ptr_callback, void *ptr_argsCallback, ip_addr_t &ref_localIp) // dont call tcpip_init_init again
{
  this->rxCallback = ptr_callback;
  this->argsCallback = ptr_argsCallback;
  TCP_DRIVER_LOGGER::log("TcpDriver initialized");
}



TcpDriver::TcpDriver() {
    TCP_DRIVER_LOGGER::log("TcpDriver created");
}


someIp::TcpConnection *TcpDriver::create_client_tcp_connection(const ip_addr_t &ref_remote_ip, uint16_t remote_port, void* ptr_user_arg)
{
  TcpipCoreLock lock; 

  if(numberOfConnections == connections.size())
  {
    TCP_DRIVER_LOGGER::error("Maximum number of connections reached"); 
    return nullptr;
  }

  for (uint8_t i = 0; i < numberOfConnections; ++i) {
    if (connections[i].pcb && ip_addr_cmp(&connections[i].pcb->remote_ip, &ref_remote_ip) && connections[i].pcb->local_port == remote_port) { //change local to conns port 
      TCP_DRIVER_LOGGER::debug("Connection already exists");
      return &connections[i];
    }
  }

  LWIP_UNUSED_ARG(ptr_user_arg); // per-pcb arg carries the connection slot (C-10)

  struct tcp_pcb* ptr_pcb = tcp_new();
  if(!ptr_pcb){
    TCP_DRIVER_LOGGER::error("tcp_new failed");
    return nullptr;
  }

  // store in a stable slot, then point callbacks at it
  size_t idx = numberOfConnections++;
  TcpConnection conn;
  conn.pcb = ptr_pcb;
  connections[idx] = std::move(conn);
  TcpConnection* ptr_slot = &connections[idx];

  tcp_arg(ptr_pcb, ptr_slot);
  tcp_recv(ptr_pcb, tcpRxWrapper);
  tcp_err(ptr_pcb, tcpErrWrapper); // clean up the slot on connection error (C-10)

  err_t err = tcp_connect(ptr_pcb, &ref_remote_ip, remote_port, tcpConnectedWrapper);
  if(err != ERR_OK)
  {
    TCP_DRIVER_LOGGER::error("tcp_connect failed: %s", lwip_strerr(err));
    tcp_abort(ptr_pcb); // frees the pcb, tcpErrWrapper clears slot->pcb
    ptr_slot->pcb = nullptr;
    numberOfConnections--; // reclaim the slot
    return nullptr;
  }

  TCP_DRIVER_LOGGER::log("Initiated TCP connection to %s:%d", ip4addr_ntoa(&ref_remote_ip), remote_port);
  return ptr_slot;
}


bool TcpDriver::is_connected(const ip_addr_t remote_ip, uint16_t remote_port)
{
  for(size_t i = 0; i < numberOfConnections; i++)
  {
    const auto& ref_conn = connections[i]; 
    if(ref_conn.pcb)
    { 
      if (ref_conn.pcb->remote_port == remote_port)
      {
        return true;
      } 
    }
  }
  return false; 
}


// sets up a listening socket
someIp::TcpConnection *TcpDriver::create_server_tcp_connection(uint16_t receivePort) { 
  for (uint8_t i = 0; i < numberOfConnections; ++i) {
    if (connections[i].pcb->local_port == receivePort) {
      TCP_DRIVER_LOGGER::debug("Connection already exists");
      return &connections[i];
    }
  }

  if (numberOfConnections == connections.size()) {
    TCP_DRIVER_LOGGER::error("Maximum number of connections reached");
    return nullptr;
  }

  TCP_DRIVER_LOGGER::log("Creating new TCP connection");
  TcpConnection tcpConnection(receivePort); // new unbound pcb via tcp_new
  
  {
    TcpipCoreLock lock;

    err_t err = tcp_bind(tcpConnection.pcb, IP_ADDR_ANY, receivePort);
    if(err != ERR_OK)
    {
        TCP_DRIVER_LOGGER::error("Error binding tcp pcb");
        return nullptr;
    }

    tcpConnection.pcb =  tcp_listen_with_backlog(tcpConnection.pcb, 5);

    if(tcpConnection.pcb == nullptr)
    {
      TCP_DRIVER_LOGGER::error("tcp_listen failed"); 
      return nullptr; 
    }
    tcp_arg(tcpConnection.pcb, this); // handler context is the driver
    tcp_accept(tcpConnection.pcb, tcpAcceptWrapper);
  }

  connections[numberOfConnections] = std::move(tcpConnection);
  numberOfConnections++; 
  // printf("index of connection created in listen to port (create_server_tcp_connection): %d\n", numberOfConnections-1); 
  // printf("state of connection created in listen to port (create_server_tcp_connection): %d\n", connections[numberOfConnections-1].pcb->state); //should be 1 (listen port doesnt really need to be saved since its not used for sending)
  return &connections[numberOfConnections - 1];   
}



bool TcpDriver::send_tcp_packet(ip4_addr_t &ref_destAddr, uint16_t destPort, PbufWrapper&& ref_buffer)
{
    TcpConnection* ptr_tcpConnection = find_tcp_connection(ref_destAddr, destPort);
    if (!ptr_tcpConnection || !ptr_tcpConnection->pcb) {
        TCP_DRIVER_LOGGER::error("send_tcp_packet: tcp connection or pcb is null");
        return false;
    }

    struct tcp_pcb* ptr_pcb = ptr_tcpConnection->pcb;

    // stream straight from the pbuf chain, no flatten/heap
    const u16_t total_len = ref_buffer.get()->tot_len;
    if (total_len == 0) {
        TCP_DRIVER_LOGGER::log("send_tcp_packet: nothing to send (len=0)");
        return true;
    }

    pbuf* ptr_p = ref_buffer.get();   // current source segment
    u16_t seg_off = 0;        // offset within p
    size_t sent = 0;
    bool failed = false;

    // take the core lock per iteration, never across a sleep (C-12)
    while (sent < total_len) {
        bool backoff = false;
        {
            TcpipCoreLock lock;

            // skip fully-consumed / empty source segments
            while (ptr_p && seg_off >= ptr_p->len) { ptr_p = ptr_p->next; seg_off = 0; }
            if (!ptr_p) break;

            u16_t snd_bytes_free = tcp_sndbuf(ptr_pcb);
            u16_t q_free = (TCP_SND_QUEUELEN > tcp_sndqueuelen(ptr_pcb))
                             ? (TCP_SND_QUEUELEN - tcp_sndqueuelen(ptr_pcb))
                             : 0;
            u32_t q_bytes_budget = (u32_t)q_free * (u32_t)TCP_MSS;

            if (snd_bytes_free == 0 || q_free == 0 || q_bytes_budget == 0) {
                // nothing queueable now, flush and back off outside the lock
                if (tcp_output(ptr_pcb) != ERR_OK) { failed = true; break; }
                backoff = true;
            } else {
                const u32_t MAX_CHUNK = (u32_t)4 * (u32_t)TCP_MSS;
                u32_t budget = (u32_t)total_len - (u32_t)sent;
                if (budget > snd_bytes_free) budget = snd_bytes_free;
                if (budget > q_bytes_budget) budget = q_bytes_budget;
                if (budget > MAX_CHUNK)      budget = MAX_CHUNK;

                const TcpChunk c = tcp_next_chunk(ptr_p, seg_off, budget);
                if (!c.ptr || c.len == 0) break; // chain exhausted

                err_t we = tcp_write(ptr_pcb, c.ptr, c.len, TCP_WRITE_FLAG_COPY);
                if (we == ERR_MEM) {
                    backoff = true; // retry the same chunk after a backoff
                } else if (we != ERR_OK) {
                    TCP_DRIVER_LOGGER::error("send_tcp_packet: tcp_write failed: %s", lwip_strerr(we));
                    failed = true;
                    break;
                } else {
                    sent += (size_t)c.len;
                    seg_off = (u16_t)(seg_off + c.len);
                    if (tcp_sndqueuelen(ptr_pcb) > (TCP_SND_QUEUELEN * 3) / 4) {
                        if (tcp_output(ptr_pcb) != ERR_OK) { failed = true; break; }
                    }
                }
            }
        } // release the core lock before any sleep

        if (backoff) os::sleep_ms(5);
    }

    if (failed) return false;

    // final flush to push the last enqueued data
    {
        TcpipCoreLock lock;
        if (tcp_output(ptr_pcb) != ERR_OK) {
            TCP_DRIVER_LOGGER::error("send_tcp_packet: final tcp_output failed");
            return false;
        }
    }

    return true;
}




bool TcpDriver::send_tcp_packet_pcb(TcpConnection* ptr_tcpConnection, PbufWrapper&& ref_buffer)
{
  if (!ptr_tcpConnection || !ptr_tcpConnection->pcb) {
    TCP_DRIVER_LOGGER::error("send_tcp_packet_pcb: null pcb");
    return false;
  }
  struct tcp_pcb* ptr_pcb = ptr_tcpConnection->pcb;

  pbuf* ptr_p = ref_buffer.get();
  if (!ptr_p || ptr_p->tot_len == 0) return true;

  // trace before first package is sent
  if constexpr(someIp::config::IS_IN_EVALUATION_MODE) {
    uint16_t sessionId = 0; 
    if(ptr_p->tot_len >= 12) {
      sessionId = read_u16_be(ref_buffer.get()->payload, 10);
    }
    ip_addr_t dest_ip = ptr_pcb->remote_ip; 
    uint16_t dest_po = ptr_pcb->remote_port; 
    lttng_ust_tracepoint(someip, lwip_tcp_send, ip4addr_ntoa(&dest_ip), dest_po, sessionId);
  } 
 
  
  {
    // keep the chain alive until fully acked
    pbuf_ref(ptr_p);
  }

  {
  TcpipCoreLock lock;
  
  tcp_sent(ptr_pcb, on_tcp_sent);

  
  if (ptr_tcpConnection->tx.active && ptr_tcpConnection->tx.p) {
    
    pbuf_cat(ptr_tcpConnection->tx.p, ptr_p);
  } else {
    ptr_tcpConnection->tx.p = ptr_p;
    ptr_tcpConnection->tx.sent = 0;
    ptr_tcpConnection->tx.acked = 0;
    ptr_tcpConnection->tx.active = true;
  }

  
  
    if (!pump_send(ptr_tcpConnection)) return false;
  }


  return true;
}

void TcpDriver::print_connections() {
  TcpipCoreLock lock;
  TCP_DRIVER_LOGGER::log("Current TCP connections:");
  for (size_t i = 0; i < numberOfConnections; ++i) {
    const auto& ref_c = connections[i];
    if (ref_c.pcb) {
      TCP_DRIVER_LOGGER::log("Connection %zu: local %s:%d <-> remote %s:%d, state: %d",
        i,
        ip4addr_ntoa(&ref_c.pcb->local_ip), ref_c.pcb->local_port,
        ip4addr_ntoa(&ref_c.pcb->remote_ip), ref_c.pcb->remote_port,
        ref_c.pcb->state);
    }
  }
}



void TcpDriver::reset() {
  TcpipCoreLock lock;
  for (auto& ref_c : connections) {
    if (ref_c.pcb) {
      tcp_arg(ref_c.pcb, nullptr);
      tcp_recv(ref_c.pcb, nullptr);
      tcp_sent(ref_c.pcb, nullptr);
      tcp_err(ref_c.pcb, nullptr);
      tcp_poll(ref_c.pcb, nullptr, 0);
      tcp_abort(ref_c.pcb);           // immediate free of pcb
      ref_c.pcb = nullptr;
    }
    if (ref_c.tx.p) { pbuf_free(ref_c.tx.p); ref_c.tx.p = nullptr; }
    ref_c.tx.sent = ref_c.tx.acked = 0; ref_c.tx.active = false;
    ref_c.expectedLen = 0;
    ref_c.receivedLen = 0; // receivedLen=0 marks the fixed inbuf empty
  }
  numberOfConnections = 0;
}
    
} // namespace someIp
 
