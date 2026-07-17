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

#ifndef SOMEIP_MESSAGEID_HPP
#define SOMEIP_MESSAGEID_HPP

#include <cstdint>

namespace someIp {

#pragma pack(push, 1)

// unique in the whole system
class MessageId {

  public:
  uint16_t _service_id;

  uint16_t _method_id;

  // combine serviceId and methodId into a uint32_t
  static uint32_t make_key(uint16_t serviceId, uint16_t methodId);

  uint32_t make_key();

  static uint16_t extract_service_id(uint32_t key);
  static uint16_t extract_method_id(uint32_t key);

  MessageId() = default;
  ~MessageId() = default;
};
#pragma pack(pop)

}

#endif //SOMEIP_MESSAGEID_HPP
