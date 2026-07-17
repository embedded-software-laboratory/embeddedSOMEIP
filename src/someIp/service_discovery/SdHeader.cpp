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

#include "SdHeader.hpp"
#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/logging/BaseLogger.hpp"

#include <cstdio>

constexpr char SD_HEADER_TAG[] = "SD_HEADER";
using SD_HEADER_LOGGER = BaseLogger<SD_HEADER_TAG>;
namespace someIp
{
    namespace sd
    {

        SdHeader::SdHeader()
        {
            _message_id._service_id = SD_MSG_SERVICE_ID;
            _message_id._method_id = SD_MSG_METHOD_ID;
            _request_id.client_id = SD_MSG_CLIENT_ID;
            _request_id.session_id = SD_MSG_SESSION_ID;
            _protocol_version = SD_MSG_PROTOCOL_VERSION;
            _interface_version = SD_MSG_INTERFACE_VERSION;
            /* [PRS_SOMEIPSD_00163] */
            _message_type = MessageType::NOTIFICATION;

            _return_code = ReturnCode::E_OK;
            // 1 flag + 3 reserved + 4 entries-array-length + 4 options-array-length
            _length += 12;
        }

        void SdHeader::increment_length(uint8_t length)
        {
            _length += length;
        }

        void SdHeader::print()
        {
            SD_HEADER_LOGGER::debug("SOMEIP-SD Header Information:");
            SD_HEADER_LOGGER::debug("Service ID: %d", _message_id._service_id);
            SD_HEADER_LOGGER::debug("Method ID: %d", _message_id._method_id);
            SD_HEADER_LOGGER::debug("Length: %d", _length);
            SD_HEADER_LOGGER::debug("Client ID: %d", _request_id.client_id);
            SD_HEADER_LOGGER::debug("Session ID: %d", _request_id.session_id);
            SD_HEADER_LOGGER::debug("Protocol Version: %d", static_cast<int>(_protocol_version));
            SD_HEADER_LOGGER::debug("Interface Version: %d", static_cast<int>(_interface_version));
            SD_HEADER_LOGGER::debug("Message Type: %d", static_cast<int>(_message_type));
            SD_HEADER_LOGGER::debug("Return Code: %d", static_cast<int>(_return_code));
        }

        void SdHeader::serialize(Serializer &ref_serializer) const
        {
            ref_serializer.serialize(_message_id._service_id);
            ref_serializer.serialize(_message_id._method_id);
            ref_serializer.serialize(_length);
            ref_serializer.serialize(_request_id.client_id);
            ref_serializer.serialize(_request_id.session_id);
            ref_serializer.serialize(_protocol_version);
            ref_serializer.serialize(_interface_version);
            ref_serializer.serialize(static_cast<uint8_t>(_message_type));
            ref_serializer.serialize(static_cast<uint8_t>(_return_code));
        }

        bool SdHeader::deserialize(Deserializer &ref_deserializer)
        {
            // uint8_t temp;
            // deserializer.peak(temp, 1);
            // std::cout << "temp 1:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 2);
            // std::cout << "temp 2:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 3);
            // std::cout << "temp 3:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 4);
            // std::cout << "temp 4:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 5);
            // std::cout << "temp 5:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 6);
            // std::cout << "temp 6:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 7);
            // std::cout << "temp 7:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 8);
            // std::cout << "temp 8:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 9);
            // std::cout << "temp 9:       ------>        " << (int)temp << std::endl;
            // deserializer.peak(temp, 10);
            // std::cout << "temp 10:       ------>        " << (int)temp << std::endl;
            bool successful = true;
            successful = successful && ref_deserializer.deserialize(_message_id._service_id);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR3\n");
            }
            successful = successful && ref_deserializer.deserialize(_message_id._method_id);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR4\n");
            }
            successful = successful && ref_deserializer.deserialize(_length);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR5\n");
            }
            successful = successful && ref_deserializer.deserialize(_request_id.client_id);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR6\n");
            }
            successful = successful && ref_deserializer.deserialize(_request_id.session_id);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR7\n");
            }
            successful = successful && ref_deserializer.deserialize(_protocol_version);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR8\n");
            }
            successful = successful && ref_deserializer.deserialize(_interface_version);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR9\n");
            }
            uint8_t tmp;
            successful = successful && ref_deserializer.deserialize(tmp);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR10\n");
            }
            _message_type = static_cast<MessageType>(tmp);
            successful = successful && ref_deserializer.deserialize(tmp);
            if (!successful)
            {
                printf("DESERIALIZATION ERROR11\n");
            }
            _return_code = static_cast<ReturnCode>(tmp);

            return successful;
        }
    } // namespace sd
} // namespace someIp
