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
#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/logging/BaseLogger.hpp"


static constexpr auto MAX_RUN_OPTIONS = 16;
constexpr char ENTRY_TAG[] = "ENTRY";
using SD_ENTRY_LOGGER = BaseLogger<ENTRY_TAG>;

namespace someIp
{
    namespace sd
    {
        class ServiceEntry;
        class EventgroupEntry;
        Entry::Entry(SdEntryType type,
                     uint16_t service_id,
                     uint16_t instance_id,
                     uint8_t major_version,
                     uint32_t ttl)
            : m_type(static_cast<uint8_t>(type)),
              m_service_id(service_id),
              m_instance_id(instance_id),
              m_major_version(major_version),
              m_ttl(ttl)
        {
            /* [PRS_SOMEIPSD_00343] */
            m_index[0] = 0x00;
            m_index[1] = 0x00;
            m_num_options[0] = 0x00;
            m_num_options[1] = 0x00;
        }

        void Entry::add_option(bool run)
        {
            m_num_options[run]++;
        }

        void Entry::serialize(Serializer &ref_serializer) const
        {
            SD_ENTRY_LOGGER::debug("Serializing service entry...");
            SD_ENTRY_LOGGER::debug("----------------------------------------------------------------------------");
            SD_ENTRY_LOGGER::debug("Entry type: ");
            print_type();
            SD_ENTRY_LOGGER::debug("Index Run 0: ", static_cast<int>(m_index[0]));
            SD_ENTRY_LOGGER::debug("Index Run 1: ", static_cast<int>(m_index[1]));
            SD_ENTRY_LOGGER::debug("Number of Options Run 0: ", static_cast<int>(m_num_options[0]));
            SD_ENTRY_LOGGER::debug("Number of Options Run 1: ", static_cast<int>(m_num_options[1]));
            SD_ENTRY_LOGGER::debug("----------------------------------------------------------------------------");
            SD_ENTRY_LOGGER::debug("Service ID: ", m_service_id);
            SD_ENTRY_LOGGER::debug("Instance ID: ", m_instance_id);
            SD_ENTRY_LOGGER::debug("Major version: ", static_cast<int>(m_major_version));
            SD_ENTRY_LOGGER::debug("TTL: ", m_ttl);
            ref_serializer.serialize(m_type);
            ref_serializer.serialize(m_index[0]);
            ref_serializer.serialize(m_index[1]);
            /* serialize 4-bits num_options[0] + 4-bits num_options[1] into one 8-bit */
            ref_serializer.serialize_4(m_num_options);
            ref_serializer.serialize(m_service_id);
            ref_serializer.serialize(m_instance_id);
            ref_serializer.serialize(m_major_version);
            ref_serializer.serialize_24(m_ttl);
        }

        bool Entry::deserialize(Deserializer &ref_deserializer)
        {
            bool successful = true;
            successful = successful && ref_deserializer.deserialize(m_type);
            successful = successful && ref_deserializer.deserialize(m_index[0]);
            successful = successful && ref_deserializer.deserialize(m_index[1]);
            successful = successful && ref_deserializer.deserialize_4(m_num_options);
            successful = successful && ref_deserializer.deserialize(m_service_id);
            successful = successful && ref_deserializer.deserialize(m_instance_id);
            successful = successful && ref_deserializer.deserialize(m_major_version);
            successful = successful && ref_deserializer.deserialize_24(m_ttl);
            SD_ENTRY_LOGGER::debug("-------------------------------------------------------------------------------");
            SD_ENTRY_LOGGER::debug("Entry Type: ");
            print_type();
            SD_ENTRY_LOGGER::debug("Index Run 0: %d", static_cast<int>(m_index[0]));
            SD_ENTRY_LOGGER::debug("Index Run 1: %d", static_cast<int>(m_index[1]));
            SD_ENTRY_LOGGER::debug("Number of Options Run 0: %d", static_cast<int>(m_num_options[0]));
            SD_ENTRY_LOGGER::debug("Service ID: %d", m_service_id);
            SD_ENTRY_LOGGER::debug("Instance ID: %d", m_instance_id);
            SD_ENTRY_LOGGER::debug("Major Version: %d", static_cast<int>(m_major_version));
            SD_ENTRY_LOGGER::debug("TTL: %d", m_ttl);
            return successful;
        }

        void Entry::print_type() const
        {
            switch (m_type)
            {
            case static_cast<uint8_t>(SdEntryType::FIND_SERVICE):
                SD_ENTRY_LOGGER::debug("FIND_SERVICE ");
                break;
            case static_cast<uint8_t>(SdEntryType::OFFER_SERVICE):
                SD_ENTRY_LOGGER::debug("OFFER_SERVICE ");
                break;
            case static_cast<uint8_t>(SdEntryType::SUBSCRIBE_EVENTGROUP):
                SD_ENTRY_LOGGER::debug("SUBSCRIBE_EVENTGROUP ");
                break;
            case static_cast<uint8_t>(SdEntryType::SUBSCRIBE_EVENTGROUP_ACK):
                SD_ENTRY_LOGGER::debug("SUBSCRIBE_EVENTGROUP_ACK ");
                break;
            default:
                SD_ENTRY_LOGGER::debug("UNKNOWN_ENTRY_TYPE ");
                break;
            }
        }
    }
}
