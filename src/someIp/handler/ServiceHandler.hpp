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

#ifndef SOMEIP_SERVICE_HANDLER_HPP
#define SOMEIP_SERVICE_HANDLER_HPP

#include "os/Os.hpp"
#include <cstdint> // uint16_t

#include "someIp/structs/Service.hpp"
#include "someIp/handler/IServiceHandler.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/config/StackConfig.hpp"
#include "someIp/pattern/Singleton.hpp"

namespace someIp
{

  // manages lookup of SOME/IP request/response services. Services are stored by
  // value in fixed inline storage (no heap).
  class ServiceHandler : public IServiceHandler, Singleton<ServiceHandler>
  {
  public:
    static ServiceHandler* get_instance()
    {
      return Singleton<ServiceHandler>::get_instance();
    }

    // returns a pointer into the inline service store, or nullptr if not found
    someIp::Service *find_service_by_id(uint16_t serviceId) override;

    // false if a service with this id already exists or capacity is reached
    bool register_service(Service newService) override;

    void deregister_service(uint16_t serviceId) override;

    ~ServiceHandler() override; // public for Singleton pattern

    ServiceHandler(); // public so an eSomeIP can own one (per-eSomeIP, not a singleton)

  private:
    StaticVector<Service, config::MAX_SERVICES> m_services;

    os::Mutex m_mutex;

    // caller must already hold the mutex
    someIp::Service *find_service_without_lock(uint16_t serviceId);

    friend class Singleton<ServiceHandler>;
  };
}

#endif // SOMEIP_SERVICE_HANDLER_HPP
