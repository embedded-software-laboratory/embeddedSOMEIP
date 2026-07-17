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

#include "someIp/config/logging_config.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "Service.hpp"

constexpr char SERVICE_TAG[] = "SERVICE";
using SERVICE_LOGGER = BaseLogger<SERVICE_TAG>;

namespace someIp
{

  Method *Service::find_method_by_id(uint16_t methodId)
  {
    for (size_t i = 0; i < m_methods.size(); ++i)
    {
      if (m_methods[i].get_id() == methodId)
      {
        return &m_methods[i];
      }
    }
    return nullptr;
  }

  bool Service::register_method(Method method)
  {
    if (find_method_by_id(method.get_id()) != nullptr)
    {
      SERVICE_LOGGER::error("Duplicate!\n");
      return false; // Method with this Id is already registered
    }
    if (m_methods.full())
    {
      SERVICE_LOGGER::error("Can't register Method with ID %d on Service %d. Reason: storage full", method.get_id(), m_id);
      return false;
    }
    SERVICE_LOGGER::log("Successfully registered Method with ID %d on Service %d", method.get_id(), m_id);
    m_methods.push_back(std::move(method));
    return true;
  }

  void Service::deregister_method(uint16_t methodId)
  {
    for (auto ptr_it = m_methods.begin(); ptr_it != m_methods.end(); ++ptr_it)
    {
      if (ptr_it->get_id() == methodId)
      {
        m_methods.erase(ptr_it);
        return;
      }
    }
  }

}
