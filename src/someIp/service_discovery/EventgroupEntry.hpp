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
#include "Option.hpp"
#include "IpOption.hpp"

#include <memory>

#ifndef SOMEIP_SD_EVENTGROUP_ENTRY_HPP_
#define SOMEIP_SD_EVENTGROUP_ENTRY_HPP_

namespace someIp
{
    namespace sd
    {

        class EventgroupEntry : public Entry
        {
        private:
            uint8_t m_counter;
            uint16_t m_eventgroup_id;
            OptionVec m_options;

        public:
            EventgroupEntry(SdEntryType type, const SdEventgroupInfo &ref_event_info);
            EventgroupEntry() {};
            ~EventgroupEntry() {};

            void set_eventgroup_id(uint16_t eventgroup_id) { m_eventgroup_id = eventgroup_id; };
            uint16_t get_eventgroup_id() const { return m_eventgroup_id; };

            SdEventgroupInfo get_eventgroup_info() const
            {
                return SdEventgroupInfo(m_service_id, m_instance_id, m_major_version, m_eventgroup_id, m_ttl);
            }

            void update_options(const OptionVec &ref_options) { m_options = ref_options; }

            OptionVec &get_options() { return m_options; }

            virtual bool equals(const someIp::PoolPtr<Entry> &ref_other) const;

            void serialize(Serializer &ref_serializer) const;
            bool deserialize(Deserializer &ref_deserializer);
        };
    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_EVENTGROUP_ENTRY_HPP_
