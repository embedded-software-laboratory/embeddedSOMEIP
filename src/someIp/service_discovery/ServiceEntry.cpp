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

#include "ServiceEntry.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/logging/BaseLogger.hpp"


constexpr char SERVICE_ENTRY_TAG[] = "SERVICE_ENTRY";
using SERVICE_ENTRY_LOGGER = BaseLogger<SERVICE_ENTRY_TAG>;

namespace someIp
{
    namespace sd
    {
        ServiceEntry::ServiceEntry(SdEntryType type,
                                     const SdServiceInfo &ref_service_info)
            : Entry(type,
                    ref_service_info._service_id,
                    ref_service_info._instance_id,
                    ref_service_info._major_version,
                    ref_service_info._ttl)
        {
            m_minor_version = ref_service_info._minor_version;
        }

        bool ServiceEntry::equals(const someIp::PoolPtr<Entry> &ref_other) const
        {
            const ServiceEntry *ptr_other_service = (ref_other && is_service_entry(ref_other->get_type()))
                ? static_cast<const ServiceEntry *>(ref_other.get())
                : nullptr;
            if (!ptr_other_service)
                return false;
            // std::cout << "----------------------------------------------------------------------------" << std::endl;
            // std::cout << "Comparing service entries:" << std::endl;
            // std::cout << "Service: " << this->get_service() << " == " << other_service->get_service() << std::endl;
            // std::cout << "Type: " << static_cast<int>(this->get_type()) << " == " << static_cast<int>(other_service->get_type()) << std::endl;
            // std::cout << "Instance: " << this->get_instance() << " == " << other_service->get_instance() << std::endl;
            // std::cout << "Major Version: " << static_cast<int>(this->get_major_version()) << " == " << static_cast<int>(other_service->get_major_version()) << std::endl;
            // std::cout << "Minor Version: " << this->get_minor_version() << " == " << other_service->get_minor_version() << std::endl;
            return this->get_type() == ptr_other_service->get_type() &&
                   this->get_service() == ptr_other_service->get_service() &&
                   this->get_instance() == ptr_other_service->get_instance() &&
                   this->get_major_version() == ptr_other_service->get_major_version() &&
                   this->get_minor_version() == ptr_other_service->get_minor_version();
        }

        void ServiceEntry::serialize(Serializer &ref_serializer) const
        {

            Entry::serialize(ref_serializer);
            // std::cout << "Minor version: " << _minor_version << std::endl;
            ref_serializer.serialize(m_minor_version);
            SERVICE_ENTRY_LOGGER::trace("Serialized service entry with minor version: %d", m_minor_version);
        }

        bool ServiceEntry::deserialize(Deserializer &ref_deserializer)
        {
            bool successful = true;
            successful = Entry::deserialize(ref_deserializer);
            if (!successful)
            {
                SERVICE_ENTRY_LOGGER::error("Failed to deserialize base entry fields");
                return false;
            }
            successful = successful && ref_deserializer.deserialize(m_minor_version);
            if (!successful)
            {
                SERVICE_ENTRY_LOGGER::error("mismatch occured, couldn't deserialize service entry");
            }
            SERVICE_ENTRY_LOGGER::debug("Minor Version: %d", m_minor_version);
            return successful;
        }
    } // namespace sd
} // namespace someIp
