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

#include "Entry.hpp"
#include "someIp/utils/PoolPtr.hpp"

#ifndef SOMEIP_SD_SERVICE_ENTRY_HPP_
#define SOMEIP_SD_SERVICE_ENTRY_HPP_

namespace someIp
{
    class Serializer;
    namespace sd
    {

        class ServiceEntry : public Entry
        {
        private:
            uint32_t m_minor_version;

        public:
            ServiceEntry(SdEntryType type, const SdServiceInfo &ref_service_info);
            ServiceEntry() {};
            ~ServiceEntry() {};

            uint32_t get_minor_version() const { return m_minor_version; }

            SdServiceInfo get_service_info() const { return SdServiceInfo(m_service_id, m_instance_id, m_major_version, m_minor_version, m_ttl); }

            virtual bool equals(const someIp::PoolPtr<Entry> &ref_other) const;

            void serialize(Serializer &ref_serializer) const;
            bool deserialize(Deserializer &ref_deserializer);
        };
    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_SERVICE_ENTRY_HPP_
