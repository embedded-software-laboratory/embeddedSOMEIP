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

#include <utility> // std::move

#include "ServiceHandler.hpp"
#include "someIp/os/Os.hpp"
#include "someIp/config/logging_config.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char SERVICE_HANDLER_TAG[] = "SERVICE_HANDLER";
using SERVICE_HANDLER_LOGGER = BaseLogger<SERVICE_HANDLER_TAG>;

namespace someIp
{

  ServiceHandler::ServiceHandler() = default;

  ServiceHandler::~ServiceHandler() = default;

  someIp::Service *ServiceHandler::find_service_by_id(uint16_t serviceId)
  {
    os::Guard lock(m_mutex);
    return find_service_without_lock(serviceId);
  }

  someIp::Service *ServiceHandler::find_service_without_lock(uint16_t serviceId)
  {
    SERVICE_HANDLER_LOGGER::debug("find_service_by_id with ID %04X", serviceId);
    for (size_t i = 0; i < m_services.size(); ++i)
    {
      if (m_services[i].get_id() == serviceId)
      {
        return &m_services[i];
      }
    }
    return nullptr;
  }

  bool ServiceHandler::register_service(Service newService)
  {
    os::Guard lock(m_mutex);
    SERVICE_HANDLER_LOGGER::log("Registering Service with ID %04X", newService.get_id());
    if (find_service_without_lock(newService.get_id()) != nullptr)
    {
      SERVICE_HANDLER_LOGGER::error("Service with ID %04X already registerd!", newService.get_id());
      return false; // Service with this Id is already registered
    }
    if (m_services.full())
    {
      SERVICE_HANDLER_LOGGER::error("No space for this Service. Remove a Service or add more space");
      return false; // storage is full
    }
    const uint16_t registeredId = newService.get_id();
    m_services.push_back(std::move(newService));
    SERVICE_HANDLER_LOGGER::log("Service with ID %04X registered", registeredId);
    return true;
  }

  void ServiceHandler::deregister_service(uint16_t serviceId)
  {
    os::Guard lock(m_mutex);
    for (auto ptr_it = m_services.begin(); ptr_it != m_services.end(); ++ptr_it)
    {
      if (ptr_it->get_id() == serviceId)
      {
        m_services.erase(ptr_it);
        return;
      }
    }
  }

}
