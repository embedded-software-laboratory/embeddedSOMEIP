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

#include <memory>

#include "Parser.hpp"
#include "RxHandler.hpp"
#include "someIp/ESomeIp.hpp"
#include "someIp/config/config.hpp"
#include "someIp/config/logging_config.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/PackageRxHandleResult.hpp"

constexpr char RX_HANDLER_TAG[] = "RX_HANDLER";
using RX_HANDLER_LOGGER = BaseLogger<RX_HANDLER_TAG>;

namespace someIp {
namespace handler {

RxHandler::RxHandler(const RxContext *ptr_rxContext, IServiceHandler *ptr_serviceHandler) {
  this->m_ptr_rxContext = ptr_rxContext;
  this->m_ptr_serviceHandler = ptr_serviceHandler;
};

Service *RxHandler::find_service(uint16_t serviceId) {
  if constexpr (someIp::config::DEBUG_LOG_ENABLED) {
    if (!m_ptr_serviceHandler) {
      RX_HANDLER_LOGGER::log("ServiceHandler is null");
    }
  }
  return m_ptr_serviceHandler->find_service_by_id(serviceId);
}

Method *RxHandler::find_method(Service *ptr_service, uint16_t methodId) {
  if (!ptr_service) {
    RX_HANDLER_LOGGER::error(
        "find_method: received nullptr instead of a service");
    return nullptr;
  }
  RX_HANDLER_LOGGER::debug("find_method with method ID %04X", methodId);
  return ptr_service->find_method_by_id(methodId);
}

PackageRxHandleResult
RxHandler::handle_rx_package(PackageRx &&ref_package) {
  // result container returned by move, no per-packet heap allocation
  PackageRxHandleResult status;
  status.code = ReturnCode::E_OK;
  status.rx = std::move(ref_package);

  status.code = status.rx.extract_header_from_payload();

  if (status.code != ReturnCode::E_OK) {
    RX_HANDLER_LOGGER::log("Not a valid SOME/IP header");
    return status;
  }

  auto header = status.rx.header;

  if (header._message_type == MessageType::ERROR) {
    RX_HANDLER_LOGGER::debug("Message Type of received package was ERROR");
    // errors are never answered with errors, so two peers cannot bounce them forever
    status.code = ReturnCode::E_NOT_OK;
    return status;
  }
  if (header._message_type == MessageType::RESPONSE) {
    // callback keyed on request identity and source endpoint
    Callback cb;
    if (ESomeIp::take_response_callback(header, status.rx.get_source_ip(), status.rx.get_source_port(), cb)) {
      RX_HANDLER_LOGGER::debug("Found session callback for session ID: %d",
                               header._request_id.session_id);
      cb(std::move(status.rx));
    } else {
      RX_HANDLER_LOGGER::log("No session callback found for session ID: %d",
                             header._request_id.session_id);
    }
    return status;
  }

  status.code = status.rx.validate_protocol_version();
  if (status.code != ReturnCode::E_OK) {
    RX_HANDLER_LOGGER::log("protocol version %02X is invalid",
                           header._protocol_version);
    return status;
  }
  RX_HANDLER_LOGGER::debug("protocol version is valid");

  status.code = status.rx.validate_interface_version();
  if (status.code != ReturnCode::E_OK) {
    RX_HANDLER_LOGGER::log("interface version %02X is invalid",
                           header._interface_version);
    return status;
  }
  RX_HANDLER_LOGGER::debug("interface version is valid");

  RX_HANDLER_LOGGER::log(
      "Received udp packet: serviceId: %04X, method/eventId: %d",
      header._message_id._service_id, header._message_id._method_id);

  auto message_id = header._message_id;
  if (header._message_type == MessageType::NOTIFICATION) {
    // notification handling, handler returned by value, no heap
    Callback callback = m_ptr_rxContext->find_event_handler(message_id._service_id,
                                                      message_id._method_id);
    if (!callback) {
      RX_HANDLER_LOGGER::log("No event handler for %04X-%d",
                             message_id._service_id, message_id._method_id);
      status.code = ReturnCode::E_UNKNOWN_METHOD;
      return status;
    }
    callback(std::move(status.rx));
    return status;
  }

  if constexpr (someIp::config::DEBUG_LOG_ENABLED) {
    if (!m_ptr_serviceHandler) {
      RX_HANDLER_LOGGER::error("serviceHandler does not exist!");
    }
  }

  auto ptr_service = find_service(status.rx.header._message_id._service_id);
  if (!ptr_service) {
    RX_HANDLER_LOGGER::log("serviceId %04X is unknown",
                           status.rx.header._message_id._service_id);
    status.code = ReturnCode::E_UNKNOWN_SERVICE;
    return status;
  }
  RX_HANDLER_LOGGER::debug("service found");

  auto ptr_method = find_method(ptr_service, status.rx.header._message_id._method_id);
  if (!ptr_method) {
    RX_HANDLER_LOGGER::log("MethodId %04X is unknown",
                           status.rx.header._message_id._method_id);
    status.code = ReturnCode::E_UNKNOWN_METHOD;
    return status;
  }
  RX_HANDLER_LOGGER::debug("Method %04X found",
                           status.rx.header._message_id._method_id);

  if (status.rx.header._return_code != ReturnCode::E_OK) {
    RX_HANDLER_LOGGER::log("Received Error message -> this implementation ignores error messages");
  }

  if (!ptr_method->has_callback()) {
    RX_HANDLER_LOGGER::log("No callback found for %04X-%d",
                           status.rx.header._message_id._service_id,
                           status.rx.header._message_id._method_id);
    status.code = ReturnCode::E_UNKNOWN_METHOD;
    return status;
  }
  ptr_method->invoke(std::move(status.rx));
  return status;
}
} // namespace handler
} // namespace someIp
