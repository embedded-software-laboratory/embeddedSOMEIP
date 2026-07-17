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

#ifndef SOMEIP_HEADER_HPP
#define SOMEIP_HEADER_HPP

#include <cstdint>
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/enums/MessageType.hpp"
#include "MessageId.hpp"

namespace someIp
{

#pragma pack(push, 1)

  // differentiates parallel uses of the same method/getter/setter
  struct RequestId
  {
    uint16_t client_id;

    uint16_t session_id;

    RequestId() = default;
    ~RequestId() = default;
  };

  struct Header
  {
    MessageId _message_id;

    // bytes from Request ID until end of message
    uint32_t _length = 8;

    RequestId _request_id;

    uint8_t _protocol_version;

    uint8_t _interface_version;

    MessageType _message_type;

    ReturnCode _return_code;
  };

#pragma pack(pop)

} // namespace someIp

#endif // HEADER_HPP