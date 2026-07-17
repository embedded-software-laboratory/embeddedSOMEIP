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

#ifndef SOMEIP_MESSAGETYPE_HPP
#define SOMEIP_MESSAGETYPE_HPP

#include <cstdint>

// 0x20 bit marks TP, 0x80 bit marks response
enum class MessageType : uint8_t {
    REQUEST = 0x00,              // request expecting a response
    REQUEST_NO_RETURN = 0x01,    // fire-and-forget request
    NOTIFICATION = 0x02,         // event notification
    RESPONSE = 0x80,             // response to a request
    ERROR = 0x81,                // error response
    TP_REQUEST = 0x20,           // segmented request
    TP_REQUEST_NO_RETURN = 0x21, // segmented fire-and-forget request
    TP_NOTIFICATION = 0x22,      // segmented notification
    TP_RESPONSE = 0xa0,          // segmented response
    TP_ERROR = 0xa1              // segmented error response
};

#endif // SOMEIP_MESSAGETYPE_HPP
