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

#ifndef SOMEIP_TP_PACKAGE_HPP
#define SOMEIP_TP_PACKAGE_HPP

#include <string>
#include <memory>

#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/PacketBuffer.hpp"
#include "someIp/structs/TPHeader.hpp"
#include "someIp/structs/Package.hpp"

namespace someIp 
{
    namespace tp 
    {
        class TpPackage {
        public:
            //constructors
            TpPackage() = default;

            TpPackage(const Header& ref_regularHeader, PacketBuffer&& ref_payload, IpAddr& ref_destinationIp, uint16_t destinationPort, uint32_t offset, bool moreSegments);
            
            //destructor 
            ~TpPackage();

             


            // serialize header and payload into a buffer
            PacketBuffer serialize();
            int size();

            TpHeader tpheader;
            PacketBuffer payload;
        
            uint16_t get_session_id() const {
                return tpheader.request_id.session_id;
            }
        
            void set_session_id(uint16_t sessionId) {
                tpheader.request_id.session_id = sessionId;
            }
        
            uint16_t getDestinationPort() const {
                return m_destinationPort;
            }
        
            IpAddr getDestinationIp() const {
                return m_destinationIp;
            }

            void setDestinationIp(const IpAddr& ref_ip) {
                m_destinationIp = ref_ip;
            }
            void setDestinationPort(uint16_t port) 
            {   
                m_destinationPort = port;
            }
        
            uint32_t calculateLength() {
                if (!payload) {
                    return 12; // 8-byte base + 4-byte TP header
                }
                return 12 + this->payload.size();
            }



            // allow move semantics  
            TpPackage(TpPackage&& ref_other) noexcept
                :   tpheader(std::move(ref_other.tpheader)), 
                    payload(std::move(ref_other.payload)),
                    m_destinationIp(ref_other.m_destinationIp),
                    m_destinationPort(ref_other.m_destinationPort){}

            TpPackage& operator=(TpPackage&& ref_other) noexcept 
            {
                if (this != &ref_other) 
                {
                    tpheader = std::move(ref_other.tpheader);
                    payload = std::move(ref_other.payload);
                    m_destinationIp = ref_other.m_destinationIp;
                    m_destinationPort = ref_other.m_destinationPort;
                }
                return *this;
            }
        
        private:
            IpAddr m_destinationIp;
            uint16_t m_destinationPort;
        };


        TpPackage deep_copy(const TpPackage& ref_original);
                
    } // namespace tp
} // namespace someIp
    
#endif // SOMEIP_TP_PACKAGE_HPP




