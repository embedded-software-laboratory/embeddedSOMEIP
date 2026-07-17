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

#ifndef SOMEIP_PACKAGE_RX_HANDLE_RESULT_HPP
#define SOMEIP_PACKAGE_RX_HANDLE_RESULT_HPP

#include "someIp/enums/ReturnCode.hpp"
#include "someIp/structs/PackageRx.hpp"

namespace someIp {

struct PackageRxHandleResult
{
  ReturnCode code;
  PackageRx rx;

  PackageRxHandleResult() = default;

  // Move constructor
  PackageRxHandleResult(PackageRxHandleResult&& ref_other) noexcept
    : code(std::move(ref_other.code)), rx(std::move(ref_other.rx)) {}

  // Move assignment operator
  PackageRxHandleResult& operator=(PackageRxHandleResult&& ref_other) noexcept
  {
    if (this != &ref_other)
    {
      code = std::move(ref_other.code);
      rx = std::move(ref_other.rx);
    }
    return *this;
  }

  // Deleted copy constructor
  PackageRxHandleResult(const PackageRxHandleResult&) = delete;

  // Deleted copy assignment operator
  PackageRxHandleResult& operator=(const PackageRxHandleResult&) = delete;
};


}

#endif // SOMEIP_PACKAGE_RX_HANDLE_RESULT_HPP