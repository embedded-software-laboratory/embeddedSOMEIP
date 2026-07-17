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

#ifndef SOMEIP_ISERVICE_HANDLER_HPP
#define SOMEIP_ISERVICE_HANDLER_HPP

#include <cstdint>
#include "someIp/structs/Service.hpp"

namespace someIp
{

  class IServiceHandler
  {
  public:
    virtual ~IServiceHandler() = default;

    // returns a pointer into the handler's inline service store, or nullptr
    virtual someIp::Service *find_service_by_id(uint16_t serviceId) = 0;

    // takes ownership of the service (moved into inline storage)
    virtual bool register_service(Service newService) = 0;

    virtual void deregister_service(uint16_t serviceId) = 0;
  };
}
#endif // SOMEIP_ISERVICE_HANDLER_HPP
