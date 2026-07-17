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

#ifndef SOMEIP_SD_ENTRY_HPP_
#define SOMEIP_SD_ENTRY_HPP_

#include <cstdint>
#include "someIp/utils/PoolPtr.hpp"
#include <vector>
#include <memory>

#include "Option.hpp"
#include "Def.hpp"

namespace someIp
{
    class Serializer;
    namespace sd
    {

        class Entry
        {
        protected:
            uint8_t m_type;
            uint16_t m_service_id;
            uint16_t m_instance_id;
            uint8_t m_major_version;
            uint32_t m_ttl;
            // [PRS_SOMEIPSD_00268] 4 bits per option run
            uint8_t m_num_options[2] = {0x00, 0x00};
            uint8_t m_index[2] = {0x00, 0x00};

        public:
            Entry() {};
            Entry(SdEntryType type, uint16_t service_id, uint16_t instance_id, uint8_t major_version, uint32_t ttl);
            virtual ~Entry() = default;

            SdEntryType get_type() const { return static_cast<SdEntryType>(m_type); };

            uint16_t get_service() const { return m_service_id; };

            uint16_t get_instance() const { return m_instance_id; };

            uint8_t get_major_version() const { return m_major_version; };

            uint32_t get_ttl() const { return m_ttl; };
            void set_ttl(uint32_t ttl) { m_ttl = ttl; };

            uint8_t get_index(uint8_t run) const { return m_index[run]; };
            void set_index(uint8_t index, bool run) { m_index[run] = index; };

            uint8_t get_num_options(bool run) const { return m_num_options[run]; };
            void reset_num_options(bool run) { m_num_options[run] = 0; };

            void add_option(bool run);

            virtual bool equals(const someIp::PoolPtr<Entry> &ref_other) const = 0;
            void print_type() const;

            virtual void serialize(Serializer &ref_serializer) const;
            virtual bool deserialize(Deserializer &ref_deserializer);
        };
    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_ENTRY_HPP_
