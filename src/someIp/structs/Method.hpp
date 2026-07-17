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

#ifndef SOMEIP_METHOD_HPP
#define SOMEIP_METHOD_HPP

#include <cstdint> // uint16_t
#include <utility> // std::move

#include "someIp/Types.hpp" // Callback, PackageRx

namespace someIp{

class Method {
  public:

    // default-constructible for StaticVector storage
    Method() = default;

    Method(uint16_t id, Callback callback) : m_id(id), m_callback(std::move(callback)) {}

    explicit Method(uint16_t id) : m_id(id) {}

    uint16_t get_id() const noexcept {
      return m_id;
    }

    bool has_callback() const noexcept {
      return static_cast<bool>(m_callback);
    }

    // check has_callback() first
    void invoke(PackageRx &&ref_package) {
      m_callback(std::move(ref_package));
    }

  private:
    uint16_t m_id = 0;
    Callback m_callback;
};
}

#endif // SOMEIP_METHOD_HPP
