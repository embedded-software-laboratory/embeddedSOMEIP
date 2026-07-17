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

#include "SdPool.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include <algorithm>
#include <memory>
#include <type_traits>

#include "SdMessage.hpp"
#include "someIp/net/IpAddress.hpp"
#include "someIp/config/config.hpp"
#include "ServiceEntry.hpp"
#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "EventgroupEntry.hpp"
#include "someIp/logging/BaseLogger.hpp"
#include "ConfigurationOption.hpp"
#include "LoadBalancingOption.hpp"
#include "Ipv4Option.hpp"
#include "Ipv6Option.hpp"

constexpr char SD_MESSAGE_TAG[] = "SD_MESSAGE";
using SD_MESSAGE_LOGGER = BaseLogger<SD_MESSAGE_TAG>;

namespace someIp
{
    namespace sd
    {

        SdMessage::SdMessage() : m_header()
        {
            // [PRS_SOMEIPSD_00540] [PRS_SOMEIPSD_00255] unicast and reboot flags set
            m_flag = SD_REBOOT_FLAG | SD_UNICAST_FLAG;
        }

        SdMessage::SdMessage(SdHeader header,
                               EntryVec entries,
                               OptionVec options,
                               uint8_t flag)
            : m_header(header),
              m_flag(flag)
        {
            for (const auto &ref_e : entries) m_entries.push_back(ref_e);
            for (const auto &ref_o : options) m_options.push_back(ref_o);
            m_entries_length = entries.size() * SD_ENTRY_LENGTH;
            m_header.increment_length(m_entries_length);
            for (const auto &ref_option : options)
            {
                if (ref_option != nullptr)
                {
                    m_options_length += ref_option->get_length();
                    m_header.increment_length(ref_option->get_length());
                }
            }
        }

        bool SdMessage::get_reboot_flag() const
        {
            return m_flag & SD_REBOOT_FLAG;
        }

        void SdMessage::set_reboot_flag()
        {
            m_flag |= SD_REBOOT_FLAG;
        }

        void SdMessage::reset_reboot_flag()
        {
            m_flag &= ~SD_REBOOT_FLAG;
        }

        void SdMessage::add_option(const someIp::PoolPtr<Option> &ref_option, const someIp::PoolPtr<Entry> &ref_entry, bool run)
        {
            m_options.push_back(std::move(ref_option));
            // on-wire size = length value + 2-byte length field + 1-byte type field
            m_options_length += ref_option->get_length() + 3;
            m_header.increment_length(ref_option->get_length() + 3);
            ref_entry->add_option(run);
        }

        bool SdMessage::overwrite_entry_options(const someIp::PoolPtr<Entry> &ref_entry,
                                                 bool run,
                                                 OptionVec options)
        {
            if (options.empty())
            {
                return false;
            }

            auto ptr_it = std::find_if(m_entries.begin(), m_entries.end(), [ref_entry](const someIp::PoolPtr<Entry> &ref_ent)
                                   { return ref_ent->equals(ref_entry); });
            if (ptr_it == m_entries.end())
            {
                SD_MESSAGE_LOGGER::error("Can't assign option to given entry, entry not found");
                return false;
            }
            if (options.size() > SD_MAX_OPTIONS_RUN)
            {
                SD_MESSAGE_LOGGER::error("Can't assign options to given entry, number of options exceeds the limit of 16 options");
                return false;
            }

            ref_entry->set_index(static_cast<uint8_t>(m_options.size()), run);
            ref_entry->reset_num_options(run);
            SD_MESSAGE_LOGGER::debug("Setting index of entry to: %d", static_cast<int>(ref_entry->get_index(run)));

            SD_MESSAGE_LOGGER::debug("SIZE OF OPTIONS: %d", options.size());
            for (auto &ref_option : options)
            {
                if (ref_option == nullptr)
                {
                    SD_MESSAGE_LOGGER::error("Option is null, can't add to entry");
                    return false;
                }
                SD_MESSAGE_LOGGER::debug("Adding option of type: %d", static_cast<int>(ref_option->get_type()));
                add_option(ref_option, ref_entry, run);
            }
            SD_MESSAGE_LOGGER::debug("Added %d options to entry", options.size());
            return true;
        }

        bool SdMessage::add_entry_options(const someIp::PoolPtr<Entry> &ref_entry,
                                           bool run,
                                           OptionVec options)
        {
            if (options.empty())
            {
                return false;
            }
            auto ptr_it = std::find_if(m_entries.begin(), m_entries.end(), [ref_entry](const someIp::PoolPtr<Entry> &ref_ent)
                                   { return ref_ent->equals(ref_entry); });
            if (ptr_it == m_entries.end())
            {
                SD_MESSAGE_LOGGER::error("Can't assign option to given entry, entry not found");
                return false;
            }

            SD_MESSAGE_LOGGER::debug("entry found");
            if (ref_entry->get_num_options(run) != 0)
            {
                SD_MESSAGE_LOGGER::error("Can't assign options to given entry, entry already has options starting at index: %d", ref_entry->get_index(run));
                return false;
            }

            if (options.size() > SD_MAX_OPTIONS_RUN)
            {
                SD_MESSAGE_LOGGER::error("Can't assign options to given entry, number of options exceeds the limit of 16 options");
                return false;
            }

            ref_entry->set_index(static_cast<uint8_t>(m_options.size()), run);
            SD_MESSAGE_LOGGER::debug("Setting index of entry to: %d", static_cast<int>(ref_entry->get_index(run)));

            SD_MESSAGE_LOGGER::debug("SIZE OF OPTIONS: %d", options.size());
            for (auto &ref_option : options)
            {
                if (ref_option == nullptr)
                {
                    SD_MESSAGE_LOGGER::error("Option is null, can't add to entry");
                    return false;
                }
                SD_MESSAGE_LOGGER::debug("Adding option of type: %d", static_cast<int>(ref_option->get_type()));
                add_option(ref_option, ref_entry, run);
            }
            SD_MESSAGE_LOGGER::debug("Added %d options to entry", options.size());
            return true;
        }

        void SdMessage::add_entry(const someIp::PoolPtr<Entry> &ref_entry)
        {
            m_entries.push_back(ref_entry);
            SD_MESSAGE_LOGGER::debug("----------------------------------------------------------------------------");
            SD_MESSAGE_LOGGER::debug("Adding entry of type: %d", static_cast<int>(m_entries[0]->get_type()));
            SD_MESSAGE_LOGGER::debug("Adding entry of service id: %d", static_cast<int>(m_entries[0]->get_service()));
            SD_MESSAGE_LOGGER::debug("Adding entry of instance id: %d", static_cast<int>(m_entries[0]->get_instance()));
            SD_MESSAGE_LOGGER::debug("Adding entry of major version id: %d", static_cast<int>(m_entries[0]->get_major_version()));
            SD_MESSAGE_LOGGER::debug("Adding entry of ttl: %d", static_cast<int>(m_entries[0]->get_ttl()));

            m_header.increment_length(SD_ENTRY_LENGTH);
            m_entries_length += SD_ENTRY_LENGTH;
        }

        someIp::PoolPtr<Entry> SdMessage::get_entry(Deserializer &ref_deserializer) const
        {
            uint8_t entry_type = 0xFF; // default init, peak() may fail (report M-9)
            if (!ref_deserializer.peak(entry_type, 1))
            {
                SD_MESSAGE_LOGGER::error("get_entry: peek past end of buffer");
                return nullptr;
            }
            if (entry_type == 0x00 || entry_type == 0x01)
            {
                return pool_make<ServiceEntry>();
            }

            else if (entry_type == 0x06 || entry_type == 0x07)
            {
                SD_MESSAGE_LOGGER::debug("Creating eventgroup entry of type: ");
                entry_type == 0x06 ? SD_MESSAGE_LOGGER::debug("SUBSCRIBE_EVENTGROUP") : SD_MESSAGE_LOGGER::debug("SUBSCRIBE_EVENTGROUP_ACK");
                return pool_make<EventgroupEntry>();
            }

            else
            {
                SD_MESSAGE_LOGGER::error("Unknown entry type: %02X", entry_type);
                return nullptr;
            }
        }

        someIp::PoolPtr<Option> SdMessage::get_option(Deserializer &ref_deserializer) const
        {
            uint8_t option_type = 0xFF; // default init, peak() may fail (report M-9)
            /* 3 to skip the 2 bytes of option length field */
            if (!ref_deserializer.peak(option_type, 3))
            {
                SD_MESSAGE_LOGGER::error("get_option: peek past end of buffer");
                return nullptr;
            }

            switch (static_cast<SdOptionType>(option_type))
            {
            case SdOptionType::CONFIGURATION:
                return pool_make<ConfigurationOption>();
            case SdOptionType::LOAD_BALANCING:
                return pool_make<LoadBalancingOption>();
#if LWIP_IPV6
            case SdOptionType::IPV6_ENDPOINT:
                return pool_make<Ipv6Option>();
            case SdOptionType::IPV6_MULTICAST:
                return pool_make<Ipv6Option>(true);
#else
            case SdOptionType::IPV4_ENDPOINT:
                return pool_make<Ipv4Option>();
            case SdOptionType::IPV4_MULTICAST:
                return pool_make<Ipv4Option>(true);
#endif
            default:
                SD_MESSAGE_LOGGER::error("Unknown option type: %d", option_type);
                return nullptr;
            }
        }

        OptionVec SdMessage::get_entry_options(someIp::PoolPtr<Entry> sprt_entry) const
        {
            OptionVec options;
            if (sprt_entry == nullptr)
            {
                SD_MESSAGE_LOGGER::error("Entry is null, can't get options");
                return options;
            }

            int index_run0 = sprt_entry->get_index(0);
            int index_run1 = sprt_entry->get_index(1);

            if (index_run1 < 0 || (index_run1 >= static_cast<uint8_t>(m_options.size()) && !m_options.empty()))
            {
                SD_MESSAGE_LOGGER::error("Index out of bounds: %d", index_run1);
                return options;
            }

            if (index_run0 < 0 || (index_run0 >= static_cast<uint8_t>(m_options.size()) && !m_options.empty()))
            {
                SD_MESSAGE_LOGGER::error("Index out of bounds: %d", index_run0);
                return options;
            }

            int num_options_run0 = sprt_entry->get_num_options(0);
            int num_options_run1 = sprt_entry->get_num_options(1);
            if (num_options_run0 < 0 || num_options_run0 > static_cast<uint8_t>(m_options.size()) - index_run0)
            {
                SD_MESSAGE_LOGGER::error("Number of options for run 0 is out of bounds: %d", num_options_run0);
                return options;
            }
            if (num_options_run1 < 0 || num_options_run1 > static_cast<uint8_t>(m_options.size()) - index_run1)
            {
                SD_MESSAGE_LOGGER::error("Number of options for run 1 is out of bounds: %d", num_options_run1);
                return options;
            }
            /* get first run options */
            for (int i = index_run0; i < index_run0 + num_options_run0; ++i)
            {
                if (i < static_cast<int>(m_options.size()))
                {
                    options.push_back(m_options[i]);
                }
            }

            /* get second run options */
            for (int i = index_run1; i < index_run1 + num_options_run1; ++i)
            {
                if (i < static_cast<int>(m_options.size()))
                {
                    options.push_back(m_options[i]);
                }
            }
            return options;
        }

        void SdMessage::serialize(Serializer &ref_serializer) const
        {
            // std::cout << "----------------------------------------------------------------------------" << std::endl;
            // SD_MESSAGE_LOGGER::debug("Serializing SD Message with length: %d", _header.get_length());
            // std::cout << "----------------------------------------------------------------------------" << std::endl;

            /* serialize header */
            m_header.serialize(ref_serializer);

            /* serialize flag */
            ref_serializer.serialize(m_flag);

            // [PRS_SOMEIPSD_00261] 24-bit reserved field
            ref_serializer.serialize_24(0);

            /* serialize entries */
            SD_MESSAGE_LOGGER::debug("entries length: %d", m_entries_length);
            ref_serializer.serialize(m_entries_length);
            for (const auto &ref_entry : m_entries)
            {
                ref_entry->serialize(ref_serializer);
            }

            /* serialize options */
            ref_serializer.serialize(m_options_length);
            SD_MESSAGE_LOGGER::debug("Serliazing options with length: %d", m_options_length);
            for (const auto &ref_option : m_options)
            {
                ref_option->serialize(ref_serializer);
            }
            SD_MESSAGE_LOGGER::debug("----------------------------------------------------------------------------");
            SD_MESSAGE_LOGGER::debug("SD Message serialized successfully");
            SD_MESSAGE_LOGGER::debug("----------------------------------------------------------------------------");
        }

        bool SdMessage::deserialize(Deserializer &ref_deserializer)
        {
            SD_MESSAGE_LOGGER::debug("----------------------------------------------------------------------------");
            SD_MESSAGE_LOGGER::debug("Deserializing SD Message");
            SD_MESSAGE_LOGGER::debug("----------------------------------------------------------------------------");
            bool successful = true;

            /* deserialize header */
            successful = m_header.deserialize(ref_deserializer);
            m_header.print();

            /* deserialize flag */
            successful = successful && ref_deserializer.deserialize(m_flag);
            SD_MESSAGE_LOGGER::debug("Flag: %d", static_cast<int>(m_flag));

            /* deserialize reserved field */
            successful = successful && ref_deserializer.dump(3);

            /* deserialize entries */
            successful = successful && ref_deserializer.deserialize(m_entries_length);
            SD_MESSAGE_LOGGER::debug("Entries Length: %d", m_entries_length);
            if (!successful)
            {
                return false;
            }
            // reject lengths past the buffer or not a whole number of 16-byte entries
            if (m_entries_length > ref_deserializer.remaining() || (m_entries_length % 16) != 0)
            {
                SD_MESSAGE_LOGGER::error("invalid entries length %u; rejecting", m_entries_length);
                return false;
            }
            for (uint32_t i = 0; i < m_entries_length; i += 16)
            {
                someIp::PoolPtr<Entry> sprt_entry = get_entry(ref_deserializer);
                if (sprt_entry == nullptr)
                {
                    SD_MESSAGE_LOGGER::error("Failed to create entry from deserializer");
                    return false;
                }

                // a failed entry deserialize means the stream is malformed
                if (!sprt_entry->deserialize(ref_deserializer))
                {
                    SD_MESSAGE_LOGGER::error("entry deserialize failed; rejecting message");
                    return false;
                }
                if (m_entries.size() >= m_entries.capacity())
                {
                    SD_MESSAGE_LOGGER::error("SD message exceeds MAX_ENTRIES_PER_SD_MESSAGE; rejecting");
                    return false;
                }
                m_entries.push_back(std::move(sprt_entry));
            }
            /* deserialize options */
            successful = successful && ref_deserializer.deserialize(m_options_length);
            SD_MESSAGE_LOGGER::debug("options length: %d", m_options_length);
            if (!successful)
            {
                return false;
            }
            // bound to what is actually present so a crafted length cannot spin (M-9)
            if (m_options_length > ref_deserializer.remaining())
            {
                SD_MESSAGE_LOGGER::error("invalid options length %u; rejecting", m_options_length);
                return false;
            }
            uint16_t i = 0;
            uint8_t count = 0;
            while (i < m_options_length)
            {
                someIp::PoolPtr<Option> sprt_option = get_option(ref_deserializer);
                if (sprt_option == nullptr)
                {
                    SD_MESSAGE_LOGGER::error("Failed to create option from deserializer");
                    return false;
                }
                if (!sprt_option->deserialize(ref_deserializer))
                {
                    SD_MESSAGE_LOGGER::error("Failed to deserialize option");
                    return false;
                }
                if (m_options.size() >= m_options.capacity())
                {
                    SD_MESSAGE_LOGGER::error("SD message exceeds MAX_OPTIONS_PER_SD_MESSAGE; rejecting");
                    return false;
                }
                m_options.push_back(std::move(sprt_option));
                // advance by on-wire size, length + 2 length bytes + 1 type byte
                i += m_options[count++]->get_length() + 3;
                SD_MESSAGE_LOGGER::debug("%d bytes read from deserializer for options", i);
                SD_MESSAGE_LOGGER::debug("options length: %d", m_options_length);
            }
            SD_MESSAGE_LOGGER::debug("----------------------------------------------------------------------------");
            SD_MESSAGE_LOGGER::debug("SD Message deserialized successfully");
            SD_MESSAGE_LOGGER::debug("----------------------------------------------------------------------------");

            return successful;
        }

    } // namespace sd
} // namespace someIp
