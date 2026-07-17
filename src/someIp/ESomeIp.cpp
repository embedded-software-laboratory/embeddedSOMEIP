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

#include <time.h>
#include "someIp/utils/PoolPtr.hpp"
#include <string.h>
#include <cstdio>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <memory>

#include "handler/RxContext.hpp"

#ifdef SOMEIP_PLATFORM_STM32
#include "communication/udp/UdpDriver.hpp"
#include "someIp/communication/LwipUdpTransport.hpp"
#include "someIp/communication/LwipTcpTransport.hpp"
#include "someIp/communication/EndpointLwip.hpp"
#include "someIp/communication/PbufByteBuffer.hpp"
#else
#include "someIp/communication/LinuxUdpTransport.hpp"
#include "someIp/communication/LinuxTcpTransport.hpp"
#endif
#include "someIp/communication/PacketConv.hpp"

#include "someIp/service_discovery/SdManager.hpp"
#include "structs/Header.hpp"
#include "structs/Package.hpp"
#include "structs/PackageRx.hpp"
#include "utils/NetworkByteOrderConverter.hpp"
#include "config/logging_config.hpp"
#include "logging/BaseLogger.hpp"

#include "ESomeIp.hpp"

#include "someip_tp.h"

constexpr char API_TAG[] = "API";
using API_LOGGER = BaseLogger<API_TAG>;


namespace someIp {

SessionTable<someIp::ESomeIp::ResponseSlot, someIp::config::MAX_OUTSTANDING_REQUESTS, someIp::ESomeIp::ResponseKey> someIp::ESomeIp::m_responseTable;
uint16_t someIp::ESomeIp::m_next_session_id = 1;

// lazy mutexes so sys_mutex_new runs after sys_init (report M-14)
namespace {
someIp::os::Mutex &response_table_mutex() {
  static someIp::os::Mutex m;
  return m;
}
someIp::os::Mutex &session_id_mutex() {
  static someIp::os::Mutex m;
  return m;
}
} // namespace

uint16_t ESomeIp::get_new_session_id() {
  os::Guard lock(session_id_mutex());
  if (m_next_session_id == 0) {
    m_next_session_id++;
  }
  return m_next_session_id++;
}

// response key includes the source endpoint (report M-11)
ESomeIp::ResponseKey ESomeIp::make_response_key(const Header &ref_hdr, const IpAddr &ref_src, uint16_t src_port) {
  ResponseKey key{};
  key.service = ref_hdr._message_id._service_id;
  key.method = ref_hdr._message_id._method_id;
  key.client = ref_hdr._request_id.client_id;
  key.session = ref_hdr._request_id.session_id;
  key.src = ref_src;
  key.src_port = src_port;
  return key;
}

bool ESomeIp::store_response_callback(const Header &ref_hdr, const IpAddr &ref_src, uint16_t src_port, Callback cb) {
  const ResponseKey key = make_response_key(ref_hdr, ref_src, src_port);
  os::Guard lock(response_table_mutex());
  // same identity overwrites a still-pending request (report M-11)
  ResponseSlot *ptr_slot = m_responseTable.find(key);
  if (ptr_slot != nullptr) {
    API_LOGGER::error("overwriting an unanswered response callback for session %u", key.session);
  } else {
    ptr_slot = m_responseTable.acquire_free(key);
  }
  if (ptr_slot == nullptr) {
    API_LOGGER::error("Response callback table full (MAX_OUTSTANDING_REQUESTS); dropping callback for session %u", key.session);
    return false;
  }
  ptr_slot->cb = std::move(cb);
  return true;
}

bool ESomeIp::take_response_callback(const Header &ref_hdr, const IpAddr &ref_src, uint16_t src_port, Callback &ref_out) {
  const ResponseKey key = make_response_key(ref_hdr, ref_src, src_port);
  os::Guard lock(response_table_mutex());
  ResponseSlot *ptr_slot = m_responseTable.find(key);
  if (ptr_slot == nullptr) {
    return false;
  }
  ref_out = std::move(ptr_slot->cb);
  m_responseTable.release(key);
  return true;
}

// select the transport backend at compile time
static ESomeIp::TransportPopulator make_default_populator()
{
  return [](TransportRegistry &ref_reg){
#ifdef SOMEIP_PLATFORM_STM32
    ref_reg.emplace_udp<LwipUdpTransport>();
    ref_reg.emplace_tcp<LwipTcpTransport>();
#else
    ref_reg.emplace_udp<LinuxUdpTransport>();
    ref_reg.emplace_tcp<LinuxTcpTransport>();
#endif
  };
}

ESomeIp::ESomeIp()
  : ESomeIp(make_default_populator()) {}

ESomeIp::ESomeIp(TransportPopulator populate)
  : m_rx_context(), m_threadPool(m_transports, &m_rx_context) {

  if (populate) {
    populate(m_transports);
  }
  m_transports.set_rx_callback([this](RxPacket &&ref_pkt){ this->on_rx(std::move(ref_pkt)); });
  m_threadPool.set_owner_api(this);

  API_LOGGER::log("eSomeIP created");
  lttng_ust_tracepoint(someip, general, "API created!");

  if(someIp::is_big_endian())
  {
    API_LOGGER::log("Host system is big endian");
  }
  else{
    API_LOGGER::log("Host system is little endian");
  }
}

void ESomeIp::add_to_queue(PackageRx &&ref_p)
{
  API_LOGGER::log("rx transportmode in add_to_queue: %d", (int)ref_p.transportMode);
  API_LOGGER::log("add to queue called");
  API_LOGGER::debug("Add to ThreadPool queue (%s)", ref_p.get_ip_as_string());
  this->m_threadPool.add_buffer_to_queue(std::move(ref_p));
}

void ESomeIp::on_rx(RxPacket &&ref_pkt)
{
  // copy into a contiguous lwIP-free buffer
  PacketBuffer pb = take_packet_buffer(std::move(ref_pkt.buffer));
  if (!pb) {
    return;
  }

  IpAddr src = IpAddr::from_u32(ref_pkt.source.v4);
  const bool is_tcp = (ref_pkt.conn.kind == TransportKind::TCP);
  TcpConnection *ptr_conn = is_tcp ? static_cast<TcpConnection *>(ref_pkt.conn.handle) : nullptr;

  PackageRx rx = PackageRx(std::move(pb), src, ref_pkt.source.port, ptr_conn);
  rx.transportMode = is_tcp ? TransportKind::TCP : TransportKind::UDP;

  // the Dispatcher classifies
  add_to_queue(std::move(rx));
}

void ESomeIp::init(IpAddr local_ip) {
	if(!m_initialized) {
		API_LOGGER::log("Init SomeIP");
    m_transports.init_all(IpEndpoint(local_ip.to_u32(), 0));
    m_tpTxSessions.clear_all();
    m_scheduler.start();
    m_reassembler.set_scheduler(&m_scheduler);
    this->m_threadPool.start_threads();
		m_initialized = true;
    API_LOGGER::log("SomeIP initialized");
	}
	else {
		API_LOGGER::log("Already initialized");
	}
  API_LOGGER::log("--------------------");
}

  uint16_t ESomeIp::listen_to_port(int port)
  {
    uint16_t acquired = static_cast<uint16_t>(port);
    if(auto* ptr_udp = m_transports.get(TransportKind::UDP)){
      if(config::DYNAMIC_UNICAST_PORT){
        // bind the first free port at or above the base
        uint16_t limit = acquired + config::DYNAMIC_PORT_RANGE;
        while(acquired < limit && !ptr_udp->open_listen_port(acquired)) acquired++;
        if(acquired >= limit) API_LOGGER::error("no free unicast port in [%d, %d)", port, limit);
      } else {
        ptr_udp->open_listen_port(acquired);
      }
    }
    // outgoing app traffic sources from this port so responses come back here
    m_local_port = acquired;
    API_LOGGER::log("Listening to port %d", acquired);
    return acquired;
  }

someIp::TcpConnection* ESomeIp::listen_to_tcp_port(int port) {
#ifdef SOMEIP_PLATFORM_STM32
  printf("creating tcp connection in listen to port\n");
  TcpConnection* ptr_conn = m_ptr_tcpDriver->create_server_tcp_connection(port);
  if(ptr_conn == nullptr)
  {
    printf("conn could not be established \n");
    return nullptr;
  }
	API_LOGGER::log("Listening to TCP port %d", port);
  return ptr_conn;
#else
  // POSIX host has no lwIP TcpConnection, the transport owns the listener
  if(auto* ptr_tcp = m_transports.get(TransportKind::TCP)){
    ptr_tcp->open_listen_port(static_cast<uint16_t>(port));
  }
  return nullptr;
#endif
}

bool ESomeIp::open_tcp_listener(uint16_t port)
{
#ifdef SOMEIP_PLATFORM_STM32
  return listen_to_tcp_port(static_cast<int>(port)) != nullptr;
#else
  if(auto* ptr_tcp = m_transports.get(TransportKind::TCP)){
    return ptr_tcp->open_listen_port(port);
  }
  return false;
#endif
}


someIp::TcpConnection *ESomeIp::connect_to_tcp_server(const IpAddr& ref_remote_ip, uint16_t remote_port)
{
#ifdef SOMEIP_PLATFORM_STM32
  return m_ptr_tcpDriver->create_client_tcp_connection(ipaddr_to_lwip(ref_remote_ip), remote_port, this);
#else
  // POSIX LinuxTcpTransport connects lazily, no handle to hand back
  if(auto* ptr_tcp = m_transports.get(TransportKind::TCP)){
    ptr_tcp->connect(IpEndpoint(ref_remote_ip.to_u32(), remote_port));
  }
  return nullptr;
#endif
}

bool ESomeIp::is_tcp_connected(const IpAddr& ref_remote_ip, uint16_t remote_port)
{
#ifdef SOMEIP_PLATFORM_STM32
  return m_ptr_tcpDriver->is_connected(ipaddr_to_lwip(ref_remote_ip), remote_port);
#else
  if(auto* ptr_tcp = m_transports.get(TransportKind::TCP)){
    return ptr_tcp->is_connected(IpEndpoint(ref_remote_ip.to_u32(), remote_port));
  }
  return false;
#endif
}

someIp::TcpConnection* ESomeIp::get_first_established_peer_connection()
{
#ifdef SOMEIP_PLATFORM_STM32
  for(size_t i = 0; i < someIp::TcpDriver::connections.size(); i++)
  {
    auto* ptr_conn = &m_ptr_tcpDriver->connections[i];
    if(ptr_conn->pcb && ptr_conn->pcb->state == ESTABLISHED && ptr_conn->pcb->remote_port != 0)
    {
      return ptr_conn;
    }
  }
  return nullptr;
#else
  return nullptr;
#endif
}


namespace {
// copy the SOME/IP header out in host byte order
bool tp_copy_header(const PacketBuffer &ref_buf, Header &ref_hdr)
{
  if (!ref_buf || ref_buf.size() < sizeof(Header))
    return false;

  ref_buf.copy_out(&ref_hdr, sizeof(Header), 0);

  ref_hdr._message_id._service_id = ensure_host_order(ref_hdr._message_id._service_id);
  ref_hdr._message_id._method_id  = ensure_host_order(ref_hdr._message_id._method_id);
  ref_hdr._request_id.client_id   = ensure_host_order(ref_hdr._request_id.client_id);
  ref_hdr._request_id.session_id  = ensure_host_order(ref_hdr._request_id.session_id);
  ref_hdr._length                 = ensure_host_order(ref_hdr._length);
  return true;
}
}  // namespace

ReturnCode ESomeIp::tp_transmit(IpAddr &ref_destAddr, uint16_t destPort, PacketBuffer &&ref_buffer)
{
  if (!ref_buffer)
  {
    API_LOGGER::debug("tp_transmit: NULL payload passed");
    return ReturnCode::E_NOT_OK;
  }

  const uint32_t totLen = ref_buffer.size();
  API_LOGGER::debug("totlen of buffer to transmit in tp_transmit: %d", totLen);

  const uint32_t maxSegSize = m_ptr_tpConfig->TxNPduLength; // [SWS_SomeIpTp_00004]
  API_LOGGER::debug("maxsegsize %d", maxSegSize);

  // SDU fits into a single UDP datagram, no segmentation needed [SWS_SomeIpTp_00009]
  if (totLen <= maxSegSize)
  {
    API_LOGGER::debug("tp_transmit: SDU fits into 1 UDP PDU, noop");

    auto tx = PackageTx(std::move(ref_buffer), ref_destAddr, destPort, m_local_port);
    if (!this->m_threadPool.send_package(std::move(tx)))
    {
      API_LOGGER::error("tp_transmit: Failed to send package");
      return ReturnCode::E_NOT_OK;
    }

    API_LOGGER::log("tp_transmit: Successfully sent single udp packet");
    return ReturnCode::E_OK;
  }

  Header baseHeader{}; // [SWS_SomeIpTp_00018]
  if (!tp_copy_header(ref_buffer, baseHeader))
  {
    API_LOGGER::error("tp_transmit: SDU to short for Header (copy error)");
    return ReturnCode::E_MALFORMED_MESSAGE;
  }

  // strip the header to expose the raw payload
  if (!ref_buffer.remove_front(sizeof(Header)))
  {
    API_LOGGER::error("tp_transmit: remove_front failed");
    return ReturnCode::E_NOT_OK;
  }

  const uint32_t sduPayloadLen = ref_buffer.size(); // [SWS_SomeIpTp_00001], [SWS_SomeIpTp_00017]

  // Check if segmentation is already ongoing for this session [SWS_SomeIpTp_00016]
  const uint16_t *ptr_sessionId = &(baseHeader._request_id.session_id);
  if (m_tpTxSessions.has_active(*ptr_sessionId))
  {
    API_LOGGER::error("tp_transmit: session %u busy - reject", ptr_sessionId);
    return ReturnCode::E_NOT_OK;
  }
  if (!m_tpTxSessions.start_session(baseHeader, sduPayloadLen))
  {
    API_LOGGER::error("tp_transmit: TxSession Table full");
    return ReturnCode::E_NOT_OK;
  }

  const uint16_t burstSize = m_ptr_tpConfig->TxBurstSize;
  const uint32_t sepTimeMs = m_ptr_tpConfig->SeperationTime; // burst separation [ms]
  uint32_t offset = 0; // [SWS_SomeIpTp_00011]
  uint32_t sentInBurst = 0;

  while (offset < sduPayloadLen)
  {
    // Create next TP segment [SWS_SomeIpTp_00003], [SWS_SomeIpTp_00002]
    tp::TpPackage seg = m_tpSegmenter.segment_next(baseHeader, ref_buffer, offset, sduPayloadLen, ref_destAddr, destPort);
    API_LOGGER::debug("segment size %d", seg.payload.size());

    auto serializedPbuf = seg.serialize();
    // empty buffer means allocation failure
    if (!serializedPbuf)
    {
      API_LOGGER::error("tp_transmit: segment serialization failed - aborting TP transmit");
      m_tpTxSessions.remove_session(*ptr_sessionId);
      return ReturnCode::E_NOT_OK;
    }
    sentInBurst++;
    // serializedPbuf holds SOME/IP header, TP header and payload
    offset += serializedPbuf.size() - sizeof(tp::TpHeader);
    m_tpTxSessions.update_offset(*ptr_sessionId, offset); // [SWS_SomeIpTp_00012]

    API_LOGGER::debug("serialized buffer of segment size: %d", serializedPbuf.size());

    auto tx = PackageTx(std::move(serializedPbuf), ref_destAddr, destPort, m_local_port);
    tx.transportMode = TransportKind::UDP_TP;
    if (!this->m_threadPool.send_package(std::move(tx)))
    {
      API_LOGGER::error("tp_transmit: Failed to send package");
      return ReturnCode::E_NOT_OK;
    }

    // Handle Tx Bursting and Separation Time [SWS_SomeIpTp_00020]
    if (burstSize >= 1 && sentInBurst >= burstSize && offset < sduPayloadLen)
    {
      sentInBurst = 0;
      os::sleep_ms(sepTimeMs);
    }
  }
  API_LOGGER::debug("entire packet sent successfully by tp module");

  m_tpTxSessions.remove_session(*ptr_sessionId);
  return ReturnCode::E_OK; // [SWS_SomeIpTp_00021]
}

bool ESomeIp::send_over_tcp(PacketBuffer &&ref_buf, [[maybe_unused]] const IpAddr &ref_destIp,
                            [[maybe_unused]] uint16_t destPort, TcpConnection *ptr_tcpConn,
                            TcpNullPolicy nullPolicy)
{
  if (ptr_tcpConn == nullptr)
  {
    switch (nullPolicy)
    {
      case TcpNullPolicy::Error:
        API_LOGGER::error("TCP transport requested but tcpConn is nullptr");
        return false;
      case TcpNullPolicy::LazyClient:
#ifndef SOMEIP_PLATFORM_STM32
        // POSIX backend lazily opens a client connection keyed by the destination
        if (auto *ptr_tcp = m_transports.get(TransportKind::TCP))
        {
          ptr_tcp->send_to(IpEndpoint(ref_destIp.to_u32(), destPort), packet_to_transport(std::move(ref_buf)), 0);
          return true;
        }
        API_LOGGER::error("no TCP transport registered");
        return false;
#else
        API_LOGGER::error("TCP transport requested but tcpConn is nullptr");
        return false;
#endif
      case TcpNullPolicy::AllowNull:
        break; // sender resolves by destination when the connection is null
    }
  }

  auto tx = PackageTx(std::move(ref_buf), ptr_tcpConn);
  tx.transportMode = TransportKind::TCP;
  return m_threadPool.send_package(std::move(tx));
}

void ESomeIp::fire_and_forget(Package &&ref_package, TransportKind transportKind, TcpConnection* ptr_tcpConn)
{
  TransportKind realKind = (transportKind == TransportKind::DEFAULT) ? this->m_transportMode : transportKind;
  API_LOGGER::log("Fire and forget");
  IpAddr destIp = ref_package.getDestinationIp();
  uint16_t destPort = ref_package.getDestinationPort();
  lttng_ust_tracepoint(someip, api_fire_and_forget, destIp.c_str(), ref_package.getDestinationPort());

  auto buf = ref_package.serialize();

  API_LOGGER::debug("payload in fire&forget: %d (+16byte header)", buf.size());

  switch (realKind)
  {
    case TransportKind::UDP:
      if(auto* ptr_udp = m_transports.get(TransportKind::UDP)){
        // honor the package's source port (0 = ephemeral) so subscribers accept the event
        ptr_udp->send_to(IpEndpoint(destIp.to_u32(), destPort), packet_to_transport(std::move(buf)), ref_package.getSrcPort());
      }
      break;
    case TransportKind::UDP_TP:
    {
      ReturnCode rc = this->tp_transmit(destIp, destPort, std::move(buf));
      if(rc != ReturnCode::E_OK){
        API_LOGGER::error("FIRE_FORGET: tp_transmit rejected SDU with code %02X", rc);
      }
      else {
        API_LOGGER::log("FIRE_FORGET: tp_transmit uccessfully sent full udp packet");
      }
      break;
    }
    case TransportKind::TCP:
      send_over_tcp(std::move(buf), destIp, destPort, ptr_tcpConn, TcpNullPolicy::LazyClient);
      break;
    default:
      API_LOGGER::error("FIRE_FORGET: Unkown Transport kind in fire&forget");
      break;
  }
}



void ESomeIp::request_and_response(Package &&ref_package, Callback callback, TransportKind transportKind, TcpConnection* ptr_tcpConn)
{
  TransportKind realKind = (transportKind == TransportKind::DEFAULT) ? this->m_transportMode : transportKind;
  API_LOGGER::log("Request and response");
  IpAddr destIp = ref_package.getDestinationIp();
  uint16_t destPort = ref_package.getDestinationPort();
  ref_package.set_session_id(get_new_session_id());
  lttng_ust_tracepoint(someip, api_send_request, destIp.c_str(), ref_package.getDestinationPort(), ref_package.header._request_id.session_id);

  auto buf = ref_package.serialize();
  store_response_callback(ref_package.header, destIp, destPort, std::move(callback));

  switch (realKind)
  {
    case TransportKind::UDP:
      {
        auto tx = PackageTx(std::move(buf), destIp, destPort, m_local_port);

        tx.transportMode = TransportKind::UDP;
        m_threadPool.send_package(std::move(tx));
      }
      break;
    case TransportKind::UDP_TP:
    {
      ReturnCode rc = this->tp_transmit(destIp, destPort, std::move(buf));
      if(rc != ReturnCode::E_OK){
        API_LOGGER::error("REQUEST_RESPONSE: tp_transmit rejected SDU with code %02X", rc);
      }
      else {
        API_LOGGER::log("REQUEST_RESPONSE: tp_transmit successfully sent full udp packet");
      }
      break;
    }
    case TransportKind::TCP:
      send_over_tcp(std::move(buf), destIp, destPort, ptr_tcpConn, TcpNullPolicy::Error);
      break;
    default:
      API_LOGGER::error("REQUEST_RESPONSE: Unkown Transport kind in fire&forget");
      break;
  }
}


void ESomeIp::send_response(PackageRx &&ref_request, std::string_view payload){
  API_LOGGER::log("send response for %04X-%04X", ref_request.header._message_id._service_id, ref_request.header._message_id._method_id);

  Package package = Package(ref_request.header._message_id._service_id, ref_request.header._message_id._method_id,
                            reinterpret_cast<const uint8_t *>(payload.data()), payload.size(),
                            ref_request.get_source_ip(), ref_request.get_source_port());
  package.set_session_id(ref_request.header._request_id.session_id); // response reuses the request's session id
  // echo the request client id so the client matches
  package.header._request_id.client_id = ref_request.header._request_id.client_id;
  package.header._message_type = MessageType::RESPONSE;

  IpAddr destIp = package.getDestinationIp();
  uint16_t destPort = package.getDestinationPort();
  lttng_ust_tracepoint(someip, api_send_response, destIp.c_str(), package.getDestinationPort(), package.header._request_id.session_id);

  auto buf = package.serialize();

  TransportKind realKind = (ref_request.transportMode == TransportKind::DEFAULT) ? this->m_transportMode : ref_request.transportMode;
  switch (realKind)
  {
    case TransportKind::UDP:
      {
        auto tx = PackageTx(std::move(buf), destIp, destPort, m_local_port);
        tx.transportMode = TransportKind::UDP;
        m_threadPool.send_package(std::move(tx));
      }
      break;
    case TransportKind::UDP_TP:
    {
      ReturnCode rc = this->tp_transmit(destIp, destPort, std::move(buf));
      if(rc != ReturnCode::E_OK){
        API_LOGGER::error("SEND_RESPONSE: tp_transmit rejected SDU with code %02X", rc);
      }
      else {
        API_LOGGER::log("SEND_RESPONSE: tp_transmit successfully sent full udp packet");
      }
      break;
    }
    case TransportKind::TCP:
      send_over_tcp(std::move(buf), destIp, destPort, ref_request.tcpConnection, TcpNullPolicy::AllowNull);
      break;
    default:
      API_LOGGER::error("SEND_RESPONSE: Unkown Transport kind in send_response: %d\n", (int)realKind);
      break;
  }
}



  bool ESomeIp::register_service(Service newService)
  {
    const uint16_t serviceId = newService.get_id();
    API_LOGGER::log("Registering serviceId %04X", serviceId);
    auto result = m_serviceHandler.register_service(std::move(newService));

    if constexpr (someIp::config::DEBUG_LOG_ENABLED)
    {
      if (result)
      {
        API_LOGGER::log("Service (%04X) registered", serviceId);
      }
      else
      {
        API_LOGGER::log("Service (%04X) not registered due to an error", serviceId);
      }
    }
    return result;
  }

  Service *ESomeIp::add_service(uint16_t serviceId)
  {
    if (!m_serviceHandler.register_service(Service(serviceId)))
      return nullptr;
    return m_serviceHandler.find_service_by_id(serviceId);
  }

  void ESomeIp::deregister_service(uint16_t serviceId)
  {
    API_LOGGER::log("Deregistering service...");
    m_serviceHandler.deregister_service(serviceId);
    API_LOGGER::log("Service deregistered");
  }

  Service *ESomeIp::get_service(uint16_t serviceId)
  {
    return m_serviceHandler.find_service_by_id(serviceId);
  }

bool ESomeIp::send_package(Package &&ref_p,/*, ip_addr_t &dest_ip, uint16_t dest_port*/ TransportKind transportKind, TcpConnection* ptr_tcpConn)
{
    TransportKind realKind = (transportKind == TransportKind::DEFAULT) ? this->m_transportMode : transportKind;
    IpAddr destIp = ref_p.getDestinationIp();
    uint16_t destPort = ref_p.getDestinationPort();
    printf("----------------------------------------------------------------------------\n");
    printf(" SENDING PACKAGE of size %d to dest ip (%s:%d)\n", ref_p.header._length +8, destIp.c_str(), destPort);
    printf("----------------------------------------------------------------------------\n");

    auto buf = ref_p.serialize();

    switch (realKind)
    {
      case TransportKind::UDP:
        {
          auto tx = PackageTx(std::move(buf), destIp, destPort, m_local_port);
          tx.transportMode = TransportKind::UDP;
          m_threadPool.send_package(std::move(tx));
        }
        break;
      case TransportKind::UDP_TP:
      {
        ReturnCode rc = this->tp_transmit(destIp, destPort, std::move(buf));
        if(rc != ReturnCode::E_OK){
          API_LOGGER::error("SEND_RESPONSE: tp_transmit rejected SDU with code %02X", rc);
        }
        else {
          API_LOGGER::log("SEND_RESPONSE: tp_transmit successfully sent full udp packet");
        }
        break;
      }
      case TransportKind::TCP:
        send_over_tcp(std::move(buf), destIp, destPort, ptr_tcpConn, TcpNullPolicy::Error);
        break;
      default:
        API_LOGGER::error("SEND_RESPONSE: Unkown Transport kind in fire&forget");
        break;
    }
    return true;
}

bool ESomeIp::send_tcp_package(Package &&ref_p, TcpConnection* ptr_tcpConn) //only for debugging
{
  printf("----------------------------------------------------------------------------\n");
  printf(" SENDING PACKAGE of size %d via TcpConnection (%p)\n", ref_p.header._length+8, (void*)ptr_tcpConn);
  printf("----------------------------------------------------------------------------\n");
  auto buf = ref_p.serialize();
  printf("conn in in send_pcb_package : (%p)\n", ptr_tcpConn);
#ifdef SOMEIP_PLATFORM_STM32
  printf("remote ip in conn in send_pcb_package : (%s)\n", ip4addr_ntoa(&ptr_tcpConn->pcb->remote_ip));
  printf("remote port in conn in send_pcb_package : (%d)\n", ptr_tcpConn->pcb->remote_port);
#endif
  auto tx = PackageTx(std::move(buf), ptr_tcpConn);
  printf("tx package pcb in in send_pcb_package : (%p)\n", tx.tcpConnection);
#ifdef SOMEIP_PLATFORM_STM32
  printf("tx package remote ip in send_pcb_package : (%s)\n", ip4addr_ntoa(&tx.tcpConnection->pcb->remote_ip));
  printf("tx package remote port in send_pcb_package : (%d)\n", tx.tcpConnection->pcb->remote_port);
#endif

  tx.transportMode = TransportKind::TCP;
  printf("transport mode: %d\n", (int)tx.transportMode);
  m_threadPool.send_package(std::move(tx));
  return true;
}

void ESomeIp::enable_sd(IpAddr local_ip, const sd::ServiceVec &ref_services){
  if(!m_owned_sd_manager){
    // constructed in-place (no heap)
    m_owned_sd_manager.emplace(&this->m_threadPool, local_ip, ref_services);
    this->m_ptr_sd_manager = &*m_owned_sd_manager;
    this->m_threadPool.set_sd_manager(&*m_owned_sd_manager);
    if (config::SD_INITIAL_NOTIFY_ON_SUBSCRIBE) {
      m_owned_sd_manager->set_new_subscriber_hook(
          [this](const sd::SdServiceInfo &ref_info, uint16_t eventgroupId, const sd::SdSubscriberInfo &ref_subscriber) {
            notify_subscriber_initial(ref_info, eventgroupId, ref_subscriber);
          });
    }
  }

  // open SD port directly so local_port_ stays intact
  if(auto* ptr_udp = m_transports.get(TransportKind::UDP)){
    ptr_udp->open_listen_port(config::SD_PORT);
  }
  IpAddr multicast_ip;
  IpAddr::from_string(config::SD_MULTICAST_IP, multicast_ip);
  join_multicast_group(multicast_ip);
}

bool ESomeIp::offer_service(someIp::PoolPtr<sd::SdService> sprt_service, const IpAddr &ref_src_ip, uint16_t src_port, sd::OptionVec options){
  if(!m_ptr_sd_manager){
    API_LOGGER::error("offer_service called before enable_sd()");
    return false;
  }
  return m_ptr_sd_manager->offer_service(std::move(sprt_service), ref_src_ip, src_port, std::move(options));
}

bool ESomeIp::stop_offer_service(sd::SdServiceInfo service_info){
  if(!m_ptr_sd_manager){
    return false;
  }
  return m_ptr_sd_manager->stop_offer_service(service_info);
}

void ESomeIp::stop_all_services(){
  if(m_ptr_sd_manager){
    m_ptr_sd_manager->stop_all_services();
  }
}

void ESomeIp::register_sd_service(someIp::PoolPtr<sd::SdService> sprt_service){
  if(m_ptr_sd_manager){
    m_ptr_sd_manager->register_service(std::move(sprt_service));
  }
}

void ESomeIp::register_client_service(someIp::PoolPtr<sd::SdClientService> sprt_client){
  if(m_ptr_sd_manager){
    m_ptr_sd_manager->register_client_service(std::move(sprt_client));
  }
}

bool ESomeIp::send_find_service(const sd::SdServiceInfo &ref_service_info, someIp::PoolPtr<sd::ConfigurationOption> sprt_config_option){
  if(!m_ptr_sd_manager){
    return false;
  }
  return m_ptr_sd_manager->send_find_service(ref_service_info, std::move(sprt_config_option));
}

const someIp::PoolPtr<sd::SdService> ESomeIp::find_service(const sd::SdServiceInfo &ref_service_info){
  if(!m_ptr_sd_manager){
    return nullptr;
  }
  return m_ptr_sd_manager->find_service(ref_service_info);
}

void ESomeIp::subscribe(const sd::SdEventgroupInfo &ref_eventgroup, sd::OptionVec options){
  if(m_ptr_sd_manager){
    m_ptr_sd_manager->subscribe(ref_eventgroup, std::move(options));
  }
}

void ESomeIp::unsubscribe_all(){
  if(m_ptr_sd_manager){
    m_ptr_sd_manager->unsubscribe_all();
  }
}

bool ESomeIp::get_sd_stop_flag(){
  return m_ptr_sd_manager ? m_ptr_sd_manager->get_stop_flag() : false;
}


void ESomeIp::register_event_handler(uint16_t serviceId, uint16_t eventId, Callback callback){
  m_rx_context.register_event_handler(serviceId, eventId, std::move(callback));
}

void ESomeIp::deregister_event_handler(uint16_t serviceId, uint16_t eventId){
  m_rx_context.deregister_event_handler(serviceId, eventId);
}

void ESomeIp::send_notifications(uint16_t serviceId, uint16_t eventId, const uint8_t *ptr_payload, size_t payloadLen, const sd::SubscriberVec &ref_subscribers, TransportKind transportKind){
  for (auto subscriber : ref_subscribers){
    auto endpoint = subscriber._option; // client-side endpoint from the Subscribe option
    Package package(serviceId, eventId, ptr_payload, payloadLen, endpoint._ip, endpoint._port);
    package.set_session_id(get_new_session_id());
    package.header._message_type = MessageType::NOTIFICATION;
    TcpConnection* ptr_tcpConn = nullptr;
#ifdef SOMEIP_PLATFORM_STM32
    if (transportKind == TransportKind::TCP){
       ptr_tcpConn = m_ptr_tcpDriver->find_tcp_connection(ipaddr_to_lwip(endpoint._ip), endpoint._port); // TODO store tcp connection in endpoint info to avoid lookup
       if (!ptr_tcpConn){
         API_LOGGER::log("send_notifications: No TCP connection found for subscriber %s:%d", endpoint._ip.c_str(), endpoint._port);
         m_ptr_tcpDriver->print_connections();
         continue; // no connection, skip this subscriber
      }
    }
#endif
    // POSIX TCP path sends by destination IP, tcpConn stays null
    fire_and_forget(std::move(package), transportKind, ptr_tcpConn);
  }
}

void ESomeIp::notify_subscriber_initial(const sd::SdServiceInfo &ref_info, uint16_t eventgroupId, const sd::SdSubscriberInfo &ref_subscriber) {
  if (!m_ptr_sd_manager) return;
  auto sprt_service = m_ptr_sd_manager->find_local_service(ref_info);
  if (!sprt_service)
    return;

  sd::SubscriberVec one;
  one.push_back(ref_subscriber);

  // runs on the SD RX path, send_notifications only enqueues
  for (uint16_t eventId : sprt_service->get_events_of_eventgroup(eventgroupId)) {
    const sd::Event *ptr_event = m_ptr_sd_manager->find_local_event(ref_info, eventId);
    if (!ptr_event || !ptr_event->on_change)
      continue; // only fields carry a current value worth pushing
    sd::event_payload payload = sprt_service->get_event_payload(eventId);
    if (payload.empty())
      continue; // field has no value yet
    send_notifications(ref_info._service_id, eventId, payload.data(), payload.size(), one, ptr_event->transport_kind);
  }
}

void ESomeIp::notify_event(sd::SdServiceInfo info, uint16_t eventId) {
  if (!m_ptr_sd_manager) return; // called before enable_sd() (report M-19)
  const auto sprt_service = this->m_ptr_sd_manager->find_local_service(info);
  if (!sprt_service)
    return;

  const sd::Event *ptr_event = this->m_ptr_sd_manager->find_local_event(info, eventId);
  if (!ptr_event)
    return;

  sd::event_payload payload = sprt_service->get_event_payload(eventId); // fixed-capacity copy, no heap
  sd::SubscriberVec subscribers = sprt_service->get_subscribers_of_eventgroup(ptr_event->eventgroup_id);

  send_notifications(info._service_id, eventId, payload.data(), payload.size(), subscribers, ptr_event->transport_kind);
}

void ESomeIp::notify_eventgroup(sd::SdServiceInfo info, uint16_t eventgroupId) {
  if (!m_ptr_sd_manager) return; // report M-19
  auto sprt_service = this->m_ptr_sd_manager->find_local_service(info);
  if (!sprt_service)
    return;

  sd::EventIdVec eventIds = sprt_service->get_events_of_eventgroup(eventgroupId);
  sd::SubscriberVec subscribers = sprt_service->get_subscribers_of_eventgroup(eventgroupId);

  for (uint16_t eventId : eventIds){
    sd::event_payload payload = sprt_service->get_event_payload(eventId);
    if (payload.empty())
      continue; // no payload set, do not notify

    const sd::Event *ptr_event = this->m_ptr_sd_manager->find_local_event(info, eventId);
    if (!ptr_event)
      return;

    send_notifications(info._service_id, eventId, payload.data(), payload.size(), subscribers, ptr_event->transport_kind);
  }
}

sd::event_payload ESomeIp::get_event_payload(sd::SdServiceInfo info, uint16_t eventId) {
  if (!m_ptr_sd_manager) return sd::event_payload{}; // report M-19
  auto sprt_service = this->m_ptr_sd_manager->find_local_service(info);
  if (!sprt_service)
    return sd::event_payload{};

  return sprt_service->get_event_payload(eventId);
}

void ESomeIp::set_event_payload(sd::SdServiceInfo info, uint16_t eventId, std::string_view payload){
  if (!m_ptr_sd_manager) return; // report M-19
  auto sprt_service = this->m_ptr_sd_manager->find_local_service(info);
  if (!sprt_service)
    return;

  if (sprt_service->set_event_payload(eventId, reinterpret_cast<const uint8_t *>(payload.data()), payload.size())) {
    notify_event(info, eventId);
  }
}

void ESomeIp::start_cyclic_update(sd::SdServiceInfo info, uint16_t eventgroupId, uint16_t intervalTime) {
  if (!m_ptr_sd_manager) return; // report M-19
  sd::Eventgroup *ptr_eventgroup = this->m_ptr_sd_manager->find_local_eventgroup(info, eventgroupId);
  if (!ptr_eventgroup)
    return;

  if (ptr_eventgroup->cyclic_running){
    API_LOGGER::log("START_CYCLIC_UPDATE: cyclic update is already running for eventgroupId %u\n", eventgroupId);
    return;
  }
  ptr_eventgroup->cyclic_running = true;
  // periodic notify on the SD scheduler, no dedicated thread
  ptr_eventgroup->cyclic_timer = this->m_ptr_sd_manager->schedule_periodic(
      "cyclic_eventgroup", intervalTime,
      [this, info, eventgroupId]{ notify_eventgroup(info, eventgroupId); });
}

void ESomeIp::stop_cyclic_update(sd::SdServiceInfo info, uint16_t eventgroupId) {
  if (!m_ptr_sd_manager) return; // report M-19
  sd::Eventgroup *ptr_eventgroup = this->m_ptr_sd_manager->find_local_eventgroup(info, eventgroupId);
  if (!ptr_eventgroup)
    return;

  ptr_eventgroup->cyclic_running = false;
  this->m_ptr_sd_manager->cancel_timer(ptr_eventgroup->cyclic_timer);
  ptr_eventgroup->cyclic_timer.reset();
}

  ESomeIp::~ESomeIp()
  {
    // quiesce transport RX before tearing down the thread pool (report C-6)
    m_transports.set_rx_callback([](RxPacket &&){});
    m_threadPool.stop_threads();
    API_LOGGER::log("eSomeIP destroyed");
    lttng_ust_tracepoint(someip, general, "API destroyed!");
  }

} // namespace someIp
