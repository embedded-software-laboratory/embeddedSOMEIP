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

#include "someIp/handler/Dispatcher.hpp"

#include "someIp/ESomeIp.hpp"
#include "someIp/service_discovery/SdManager.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/enums/MessageType.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/utils/NetworkByteOrderConverter.hpp"
#ifdef SOMEIP_PLATFORM_STM32
#endif
#include "someIp/communication/PacketConv.hpp"
#include "someIp/utils/ErrorCounters.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char DISPATCHER_TAG[] = "DISPATCHER";
using DISPATCHER_LOGGER = BaseLogger<DISPATCHER_TAG>;

namespace someIp {
namespace handler {

// --- TpHandler ---

bool TpHandler::can_handle(const Header &ref_hdr, TransportKind mode) const {
  // TP is UDP only, bit 5 (0x20) of the message type marks a segment
  if (mode == TransportKind::TCP) {
    return false;
  }
  return (static_cast<uint8_t>(ref_hdr._message_type) & 0x20u) != 0u;
}

void TpHandler::handle(PackageRx &&ref_pkg) {
  // re-queues the completed message with TP flag cleared for reclassification
  m_ptr_owner->reassembler()->pushFragment(std::move(ref_pkg), m_ptr_owner);
}

// --- SdHandler ---

bool SdHandler::can_handle(const Header &ref_hdr, TransportKind) const {
  // header is already host order, compare ids directly without re-swapping
  return m_ptr_mng != nullptr && *m_ptr_mng != nullptr &&
         ref_hdr._message_id._service_id == sd::SD_MSG_SERVICE_ID &&
         ref_hdr._message_id._method_id == sd::SD_MSG_METHOD_ID;
}

void SdHandler::handle(PackageRx &&ref_pkg) {
  if (ref_pkg.deserialize()) {
    sd::SdManager::recv_callback(*m_ptr_mng, *ref_pkg.msg, ref_pkg.get_source_ip(), ref_pkg.get_source_port());
  }
}

// --- StandardHandler ---

void StandardHandler::handle(PackageRx &&ref_pkg) {
  const TransportKind transport_mode = ref_pkg.transportMode;
  PackageRxHandleResult result = m_rxHandler.handle_rx_package(std::move(ref_pkg));

  // automatic error response for failed REQUESTs
  if (result.rx.header._message_type == MessageType::REQUEST &&
      result.code != ReturnCode::E_OK) {
    Package errp = Package();
    errp.header = result.rx.header;
    errp.header._return_code = result.code;
    errp.header._message_type = MessageType::ERROR;
    auto p = errp.serialize();

    if (transport_mode == TransportKind::UDP || transport_mode == TransportKind::UDP_TP) {
      if (auto *ptr_udp = m_ref_transports.get(TransportKind::UDP)) {
        ptr_udp->send_to(IpEndpoint(result.rx.get_source_ip().to_u32(), result.rx.get_source_port()),
                     packet_to_transport(std::move(p)), 0);
      }
    } else {
      if (auto *ptr_tcp = m_ref_transports.get(TransportKind::TCP)) {
        ptr_tcp->send_on(ConnectionId(TransportKind::TCP, result.rx.tcpConnection),
                     packet_to_transport(std::move(p)));
      }
    }
  }
}

// --- Dispatcher ---

Dispatcher::Dispatcher(const RxContext *ptr_rxContext, sd::SdManager **ptr_mng,
                       TransportRegistry &ref_transports, ESomeIp *ptr_owner)
    : m_tp(ptr_owner), m_sd(ptr_mng), m_standard(ptr_rxContext, ref_transports, ptr_owner->service_handler()) {}

void Dispatcher::dispatch(PackageRx &&ref_pkg) {
  Header hdr{};
  if (ref_pkg.peek_header(&hdr) != ReturnCode::E_OK) {
    // count malformed headers instead of dropping silently (M-12)
    drop_counters().inc(DropCounter::malformed_header);
    DISPATCHER_LOGGER::error("malformed/too-short SOME/IP header; dropping package");
    return;
  }
  const TransportKind mode = ref_pkg.transportMode;

  if (m_tp.can_handle(hdr, mode)) {
    m_tp.handle(std::move(ref_pkg));
    return;
  }
  if (m_sd.can_handle(hdr, mode)) {
    m_sd.handle(std::move(ref_pkg));
    return;
  }
  m_standard.handle(std::move(ref_pkg));
}

} // namespace handler
} // namespace someIp
