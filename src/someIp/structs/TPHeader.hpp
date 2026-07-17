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

// extra 4-byte TP header, only present when the TP bit is set

#ifndef SOMEIP_TP_HEADER_HPP
#define SOMEIP_TP_HEADER_HPP

#include <cstdint>
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/enums/MessageType.hpp"
#include "someIp/structs/MessageId.hpp"
#include "someIp/structs/Header.hpp"

namespace someIp {
namespace tp {

#pragma pack(push, 1)

struct TpOnlyHeader 
{
public:    
    TpOnlyHeader() : full_tponlyheader(0) {}

     
    uint32_t get_offset() const
    {
      return (full_tponlyheader & 0xFFFFFFF0) >> 4;
    }

    uint8_t get_reserved() const
    {
      return (full_tponlyheader & 0xE) >> 1;
    }

    bool get_msf() const
    {
      return (full_tponlyheader & 0x1u);
    }

    void set_offset(uint32_t offset)
    {
      full_tponlyheader = (((full_tponlyheader & 0x0000000F) | ((offset << 4) & 0xFFFFFFF0))); 
    }

    void set_reserved(uint8_t reserved)
    {
      full_tponlyheader = (((full_tponlyheader & 0xFFFFFFF1)) | ((reserved << 1) & 0xE));
    }


    void set_msf(bool msf)
    {
      full_tponlyheader = (full_tponlyheader & 0xFFFFFFFE) | (msf ? 1:0);
    }

    uint32_t full_tponlyheader = 0;
};




// SOME/IP header plus 28-bit offset, 3-bit reserved, 1-bit MSF
struct TpHeader 
{
  MessageId message_id;

  uint32_t length = 12;  // 12 includes the TP field

  RequestId request_id;

  uint8_t protocol_version;
  uint8_t interface_version;
  MessageType message_type;
  ReturnCode return_code; //extend return code types
  
  TpOnlyHeader tponlyheader;

  TpHeader() = default;
  ~TpHeader() = default;
};

#pragma pack(pop)

} // namespace tp
} // namespace someIp

#endif // SOMEIP_TP_HEADER_HPP