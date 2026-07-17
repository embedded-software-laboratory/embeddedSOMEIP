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

#include "EventgroupEntry.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/logging/BaseLogger.hpp"

constexpr char EVENTGROUP_ENTRY_TAG[] = "EVENTGROUP_ENTRY";
using EVENTGROUP_ENTRY_LOGGER = BaseLogger<EVENTGROUP_ENTRY_TAG>;

namespace someIp
{
    namespace sd
    {
        EventgroupEntry::EventgroupEntry(SdEntryType type, const SdEventgroupInfo &ref_event_info)
            : Entry(type,
                    ref_event_info._service_id,
                    ref_event_info._instance_id,
                    ref_event_info._major_version,
                    ref_event_info._ttl)
        {
            m_eventgroup_id = ref_event_info._eventgroup_id;
            m_counter = 0;
        }

        bool EventgroupEntry::equals(const someIp::PoolPtr<Entry> &ref_other) const
        {
            const EventgroupEntry *ptr_other_service = (ref_other && is_eventgroup_entry(ref_other->get_type()))
                ? static_cast<const EventgroupEntry *>(ref_other.get())
                : nullptr;
            if (!ptr_other_service)
                return false;

            return this->get_type() == ptr_other_service->get_type() &&
                   this->get_service() == ptr_other_service->get_service() &&
                   this->get_instance() == ptr_other_service->get_instance() &&
                   this->get_major_version() == ptr_other_service->get_major_version() &&
                   this->get_eventgroup_id() == ptr_other_service->get_eventgroup_id();
        }

        void EventgroupEntry::serialize(Serializer &ref_serializer) const
        {
            Entry::serialize(ref_serializer);
            /* 12-bits reserved + 4-bits counter */
            ref_serializer.serialize(static_cast<uint8_t>(0x00));
            ref_serializer.serialize(m_counter);
            ref_serializer.serialize(m_eventgroup_id);
        }

        bool EventgroupEntry::deserialize(Deserializer &ref_deserializer)
        {
            bool successful;
            successful = Entry::deserialize(ref_deserializer);
            successful = successful && ref_deserializer.dump(1);
            successful = successful && ref_deserializer.deserialize(m_counter);
            if (m_counter & 0xF0)
            {
                EVENTGROUP_ENTRY_LOGGER::error("counter is not 4-bits, deserialization error");
                successful = false;
            }
            successful = successful && ref_deserializer.deserialize(m_eventgroup_id);

            if (!successful)
            {
                EVENTGROUP_ENTRY_LOGGER::error("mismatch occured, couldn't deserialize eventgroup entry");
            }

            return successful;
        }
    } // namespace sd
} // namespace someIp
