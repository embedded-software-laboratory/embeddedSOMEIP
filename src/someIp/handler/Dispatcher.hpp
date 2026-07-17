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

#ifndef SOMEIP_HANDLER_DISPATCHER_HPP
#define SOMEIP_HANDLER_DISPATCHER_HPP

#include <memory>

#include "someIp/structs/Header.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/config/communication_config.hpp"
#include "someIp/handler/RxContext.hpp"
#include "someIp/handler/RxHandler.hpp"
#include "someIp/communication/TransportRegistry.hpp"
#include "someIp/transport-protocol/Reassembler.hpp"

namespace someIp {

class ESomeIp;
namespace sd { class SdManager; }

namespace handler {

// can_handle claims a package, handle consumes it
class IRxHandler {
public:
  virtual ~IRxHandler() = default;
  virtual bool can_handle(const Header &ref_hdr, TransportKind mode) const = 0;
  virtual void handle(PackageRx &&ref_pkg) = 0;
};

// reassembled messages are re-queued with the TP flag cleared
class TpHandler : public IRxHandler {
public:
  explicit TpHandler(ESomeIp *ptr_owner) : m_ptr_owner(ptr_owner) {}
  bool can_handle(const Header &ref_hdr, TransportKind mode) const override;
  void handle(PackageRx &&ref_pkg) override;

private:
  ESomeIp *m_ptr_owner; // owns the per-eSomeIP reassembler
};

// pointer-to-pointer because sd_manager binds after construction
class SdHandler : public IRxHandler {
public:
  explicit SdHandler(sd::SdManager **ptr_mng) : m_ptr_mng(ptr_mng) {}
  bool can_handle(const Header &ref_hdr, TransportKind mode) const override;
  void handle(PackageRx &&ref_pkg) override;

private:
  sd::SdManager **m_ptr_mng;
};

// standard request/response/notification dispatch plus automatic error response
class StandardHandler : public IRxHandler {
public:
  StandardHandler(const RxContext *ptr_rxContext, TransportRegistry &ref_transports, IServiceHandler *ptr_sh)
      : m_rxHandler(ptr_rxContext, ptr_sh), m_ref_transports(ref_transports) {}
  bool can_handle(const Header &, TransportKind) const override { return true; }
  void handle(PackageRx &&ref_pkg) override;

private:
  RxHandler m_rxHandler;
  TransportRegistry &m_ref_transports;
};

// routes each received package to the first matching handler
class Dispatcher {
public:
  // defined in the .cpp, needs the complete ESomeIp type
  Dispatcher(const RxContext *ptr_rxContext, sd::SdManager **ptr_mng,
             TransportRegistry &ref_transports, ESomeIp *ptr_owner);

  void dispatch(PackageRx &&ref_pkg);

private:
  TpHandler m_tp;
  SdHandler m_sd;
  StandardHandler m_standard;
};

} // namespace handler
} // namespace someIp

#endif // SOMEIP_HANDLER_DISPATCHER_HPP
