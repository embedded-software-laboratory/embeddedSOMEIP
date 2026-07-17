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

#ifndef SOMEIP_SD_OPTION_HPP_
#define SOMEIP_SD_OPTION_HPP_

#include <cstdint>
#include "someIp/utils/PoolPtr.hpp"
#include <vector>
#include <memory>

#include "someIp/utils/Serializer.hpp"
#include "someIp/utils/Deserializer.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/config/StackConfig.hpp"
#include "Def.hpp"

namespace someIp
{
    namespace sd
    {

        class Option
        {
        protected:
            // option size excluding length and type fields
            uint16_t m_length;
            SdOptionType m_type;
            uint8_t m_discardable_flag;

        public:
            Option() {};
            Option(SdOptionType type, bool discardable = false);
            virtual ~Option() = default;
            uint16_t get_length() const { return m_length; };
            SdOptionType get_type() const { return m_type; };
            bool is_discardable() const { return m_discardable_flag; };
            virtual void serialize(Serializer &ref_serializer) const;
            virtual bool deserialize(Deserializer &ref_deserializer);
            void print_type() const;
        };

        // fixed-capacity option list, no heap
        using OptionVec = StaticVector<someIp::PoolPtr<Option>, config::MAX_OPTIONS_PER_SD_MESSAGE>;

    } // namespace sd
} // namespace someIp

#endif // SOMEIP_SD_OPTION_HPP_
