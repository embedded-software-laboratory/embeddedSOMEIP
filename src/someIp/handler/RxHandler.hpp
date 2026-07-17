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

#ifndef SOMEIP_RXHANDLER_HPP
#define SOMEIP_RXHANDLER_HPP

#include <memory>

#include "RxContext.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/handler/RxContext.hpp"
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/PackageRxHandleResult.hpp"

namespace someIp::handler {

// handles incoming SOME/IP messages
class RxHandler {
private:
  IServiceHandler *m_ptr_serviceHandler; // owned by the eSomeIP, passed in
  const RxContext *m_ptr_rxContext;
  // returns service or nullptr if not found (pointer into the handler's store)
  Service *find_service(uint16_t serviceId);

  // returns method or nullptr if not found (pointer into the service's store)
  static Method *find_method(Service *ptr_service, uint16_t methodId);

  // checks method type matches the expected type
  static ReturnCode validate_message_Type(Method *ptr_method, uint16_t messageType);

public:
  RxHandler(const RxContext *ptr_rxContext, IServiceHandler *ptr_serviceHandler);
  ~RxHandler() = default;

  PackageRxHandleResult handle_rx_package(PackageRx &&ref_package);
};

} // namespace someIp::handler

#endif // SOMEIP_RXHANDLER_HPP
