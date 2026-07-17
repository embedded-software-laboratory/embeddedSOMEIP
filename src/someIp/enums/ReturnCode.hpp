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

#ifndef SOMEIP_RETURNCODE_HPP
#define SOMEIP_RETURNCODE_HPP

#include <cstdint>


// SOME/IP return codes per spec
enum class ReturnCode : uint8_t {
    E_OK = 0x00,                     // no error
    E_NOT_OK = 0x01,                 // unspecified error
    E_UNKNOWN_SERVICE = 0x02,        // unknown service ID
    E_UNKNOWN_METHOD = 0x03,         // unknown method ID
    E_NOT_READY = 0x04,              // application not running
    E_NOT_REACHABLE = 0x05,          // service host not reachable
    E_TIMEOUT = 0x06,                // timeout
    E_WRONG_PROTOCOL_VERSION = 0x07, // unsupported protocol version
    E_WRONG_INTERFACE_VERSION = 0x08, // interface version mismatch
    E_MALFORMED_MESSAGE = 0x09,      // payload deserialization error
    E_WRONG_MESSAGE_TYPE = 0x0a,     // unexpected message type
    E_E2E_REPEATED = 0x0b,           // repeated E2E calculation error
    E_E2E_WRONG_SEQUENCE = 0x0c,     // wrong E2E sequence error
    E_E2E = 0x0d,                    // unspecified E2E error
    E_E2E_NOT_AVAILABLE = 0x0e,      // E2E not available
    E_E2E_NO_NEW_DATA = 0x0f,        // no new data for E2E
};

#endif
