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

// SOME/IP SD message, header plus entries and options
#ifndef SOMEIP_SD_MESSAGE_HPP_
#define SOMEIP_SD_MESSAGE_HPP_

#include <vector>
#include "someIp/utils/PoolPtr.hpp"
#include <memory>

#include "Entry.hpp"
#include "SdHeader.hpp"
#include "Option.hpp"
#include "someIp/structs/PackageTx.hpp"
#include "EventgroupEntry.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp
{
    namespace sd
    {
        // bounded, no-heap storage for one SD message (OptionVec from Option.hpp)
        using EntryVec = StaticVector<someIp::PoolPtr<Entry>, config::MAX_ENTRIES_PER_SD_MESSAGE>;
        using EventgroupEntryVec = StaticVector<someIp::PoolPtr<EventgroupEntry>, config::MAX_ENTRIES_PER_SD_MESSAGE>;

        class SdMessage
        {
        private:
            SdHeader m_header;
            /* [PRS_SOMEIPSD_00253] */
            EntryVec m_entries;
            OptionVec m_options;
            uint8_t m_flag;
            uint32_t m_entries_length = 0;
            uint32_t m_options_length = 0;
            someIp::PoolPtr<Entry> get_entry(Deserializer &ref_deserializer) const;
            someIp::PoolPtr<Option> get_option(Deserializer &ref_deserializer) const;

            // add an option to an entry's run (run 0 = first, 1 = second)
            void add_option(const someIp::PoolPtr<Option> &ref_option, const someIp::PoolPtr<Entry> &ref_entry, bool run);

        public:
            SdMessage();
            SdMessage(SdHeader header,
                       EntryVec entries,
                       OptionVec options,
                       uint8_t flag = SD_ALL_FLAGS);
            ~SdMessage() {};

            bool get_reboot_flag() const;
            void set_reboot_flag();
            void reset_reboot_flag();

            // per-destination incrementing session id, set just before transmit
            void set_session_id(uint16_t sid) { m_header._request_id.session_id = sid; }

            uint32_t get_length() const { return m_header.get_length(); }

            void add_entry(const someIp::PoolPtr<Entry> &ref_entry);

            bool overwrite_entry_options(const someIp::PoolPtr<Entry> &ref_entry,
                                         bool run,
                                         OptionVec options);
            bool add_entry_options(const someIp::PoolPtr<Entry> &ref_entry, bool run, OptionVec options);

            EntryVec get_entries() const { return m_entries; }
            OptionVec get_options() const { return m_options; }
            OptionVec get_entry_options(someIp::PoolPtr<Entry> sprt_entry) const;

            void serialize(Serializer &ref_serializer) const;
            bool deserialize(Deserializer &ref_deserializer);
        };
    } // namespace sd
} // namespace someip

#endif // SOMEIP_SD_MESSAGE_HPP_
