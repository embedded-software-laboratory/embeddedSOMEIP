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

#include "MessageId.hpp"

namespace someIp
{
  uint32_t MessageId::make_key(uint16_t serviceId, uint16_t methodId) {
      return (static_cast<uint32_t>(serviceId) << 16) | methodId;
  }

  uint32_t MessageId::make_key() {
      return (static_cast<uint32_t>(_service_id) << 16) | _method_id;
  }

  uint16_t MessageId::extract_service_id(uint32_t key) {
      return static_cast<uint16_t>(key >> 16);
  }

  uint16_t MessageId::extract_method_id(uint32_t key) {
      return static_cast<uint16_t>(key & static_cast<uint32_t>(0xFFFF));
  }
} // namespace someIp
