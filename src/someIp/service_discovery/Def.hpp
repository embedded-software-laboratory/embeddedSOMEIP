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

#include <cstdint>
#include "someIp/utils/PoolPtr.hpp"
#include <memory>
#include <string>

#include "someIp/net/IpAddress.hpp"
#include "someIp/config/communication_config.hpp"
#include "someIp/config/StackConfig.hpp"
#include "someIp/communication/tcp/TcpConnection.hpp"
#include "someIp/utils/Mutex.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/utils/timer.hpp"

#ifndef SOMEIP_SD_FLAGS_HPP_
#define SOMEIP_SD_FLAGS_HPP_
namespace someIp
{
    namespace sd
    {
        class Option;

        constexpr uint16_t SD_MULTICAST_PORT = 8010;

        // reboot flag, highest order bit [PRS_SOMEIPSD_00254]
        constexpr auto SD_REBOOT_FLAG = 0x80;

        // unicast flag, next highest order bit [PRS_SOMEIPSD_00259]
        constexpr auto SD_UNICAST_FLAG = 0x40;

        constexpr auto SD_ALL_FLAGS = SD_REBOOT_FLAG | SD_UNICAST_FLAG;

        constexpr uint16_t SD_ANY_INSTANCE_ID = 0xFFFF;
        constexpr uint8_t SD_ANY_MAJOR_VERSION = 0xFF;
        constexpr uint32_t SD_ANY_MINOR_VERSION = 0xFFFFFFFF;
        constexpr uint16_t SD_ANY_EVENTGROUP = 0xFFFF;
        constexpr uint32_t SD_MAX_TTL = 0xFFFFFFFF;
        constexpr uint32_t SD_END_TTL = 0x00000000;
        constexpr uint16_t SD_MAX_SESSION_ID = 4000;

        constexpr bool CYCLIC_OFFER_DELAY_DEFINE = true;
        // constexpr uint32_t CYCLIC_OFFER_DELAY = 1000; // ms

        constexpr uint16_t SD_LOWEST_PRIORITY = 0xFFFF;

        constexpr uint8_t SUBSCRIBERS_THRESHOLD = 10;
        constexpr bool MULTICAST_THRESHOLD = 1;
        // [PRS_SOMEIPSD_00270]
        auto constexpr SD_ENTRY_LENGTH = 16;
        auto constexpr SD_MAX_OPTIONS_RUN = 16;

        constexpr bool SD_FIRST_OPTION_RUN = 0;
        constexpr bool SD_SECOND_OPTION_RUN = 1;

        // SD message header [PRS_SOMEIPSD_00151]
        constexpr uint16_t SD_MSG_SERVICE_ID = 0xFFFF;
        // [PRS_SOMEIPSD_00152]
        constexpr uint16_t SD_MSG_METHOD_ID = 0x8100;
        // [PRS_SOMEIPSD_00154]
        constexpr uint16_t SD_MSG_CLIENT_ID = 0x0000;
        // [PRS_SOMEIPSD_00158]
        constexpr uint16_t SD_MSG_SESSION_ID = 0x0001;
        // [PRS_SOMEIPSD_00161]
        constexpr uint8_t SD_MSG_PROTOCOL_VERSION = 0x01;
        // [PRS_SOMEIPSD_00162]
        constexpr uint8_t SD_MSG_INTERFACE_VERSION = 0x01;

        enum class SdOptionType : uint8_t
        {
            CONFIGURATION = 0x01,
            LOAD_BALANCING = 0x02,
            IPV4_ENDPOINT = 0x04,
            IPV6_ENDPOINT = 0x06,
            IPV4_MULTICAST = 0x14,
            IPV6_MULTICAST = 0x16,
        };

        // service discovery phases [PRS_SOMEIPSD_00405, PRS_SOMEIPSD_00411]
        enum class SdPhase : uint8_t
        {
            NOT_READY,
            READY,
            INITIAL_WAIT,   // random delay before first message
            REPETITION,     // exponential backoff phase
            MAIN            // steady state phase
        };

        enum class SdEntryType : uint8_t
        {
            FIND_SERVICE = 0x00,

            OFFER_SERVICE = 0x01,
            STOP_OFFER_SERVICE = 0x01,

            SUBSCRIBE_EVENTGROUP = 0x06,
            STOP_SUBSCRIBE_EVENTGROUP = 0x06,

            SUBSCRIBE_EVENTGROUP_ACK = 0x07,
            SUBSCRIBE_EVENTGROUP_NACK = 0x07,
        };

        // type byte is the runtime tag, no dynamic_cast needed
        inline bool is_service_entry(SdEntryType t) {
            return t == SdEntryType::FIND_SERVICE || t == SdEntryType::OFFER_SERVICE;
        }
        inline bool is_eventgroup_entry(SdEntryType t) {
            return t == SdEntryType::SUBSCRIBE_EVENTGROUP || t == SdEntryType::SUBSCRIBE_EVENTGROUP_ACK;
        }
        inline bool is_ipv4_option(SdOptionType t) {
            return t == SdOptionType::IPV4_ENDPOINT || t == SdOptionType::IPV4_MULTICAST;
        }
        inline bool is_ipv6_option(SdOptionType t) {
            return t == SdOptionType::IPV6_ENDPOINT || t == SdOptionType::IPV6_MULTICAST;
        }
        inline bool is_ip_option(SdOptionType t) {
            return is_ipv4_option(t) || is_ipv6_option(t);
        }
        inline bool is_load_balancing_option(SdOptionType t) {
            return t == SdOptionType::LOAD_BALANCING;
        }

        enum class SdTransportProtocol : uint8_t
        {
            UDP = 0x11,
            TCP = 0x06,
        };

        enum class SdInterfaceStatus : uint8_t
        {
            LINK_UP,
            LINK_DOWN
        };

        enum class SdClientState
        {
            // NOT_REQUESTED_SERVICE_NOT_SEEN,
            NOT_REQUESTED,
            REQUESTED_BUT_NOT_READY,
            SEARCH_INITIAL_WAIT_TIMER_SET,
            SEARCH_REPETITION_TIMER_SET,
            SERVICE_READY,
            STOPPED
        };

        struct SdServiceInfo
        {
            uint16_t _service_id;
            uint16_t _instance_id;
            uint8_t _major_version;
            uint32_t _minor_version;
            uint32_t _ttl = SD_MAX_TTL;
            uint16_t _priority = SD_LOWEST_PRIORITY;
            uint16_t _weight = 0;
            uint16_t _src_port = 0;

            SdServiceInfo() {};
            SdServiceInfo(uint16_t service_id,
                            uint16_t instance_id,
                            uint8_t major_version,
                            uint32_t minor_version,
                            uint32_t ttl = SD_MAX_TTL) : _service_id(service_id),
                                                         _instance_id(instance_id),
                                                         _major_version(major_version),
                                                         _minor_version(minor_version),
                                                         _ttl(ttl) {};
            SdServiceInfo(uint16_t service_id,
                            uint16_t instance_id,
                            uint8_t major_version,
                            uint32_t minor_version,
                            uint16_t src_port,
                            uint32_t ttl) : _service_id(service_id),
                                                         _instance_id(instance_id),
                                                         _major_version(major_version),
                                                         _minor_version(minor_version),
                                                         _src_port(src_port),
                                                         _ttl(ttl) {};

            SdServiceInfo(const SdServiceInfo &ref_other) : _service_id(ref_other._service_id),
                                                            _instance_id(ref_other._instance_id),
                                                            _major_version(ref_other._major_version),
                                                            _minor_version(ref_other._minor_version),
                                                            _ttl(ref_other._ttl),
                                                            _src_port(ref_other._src_port) {};

            bool operator==(const SdServiceInfo &ref_other) const
            {
                return _service_id == ref_other._service_id &&
                       (_instance_id == ref_other._instance_id ||
                        _instance_id == SD_ANY_INSTANCE_ID) &&
                       (_major_version == ref_other._major_version ||
                        _major_version == SD_ANY_MAJOR_VERSION) &&
                       (_minor_version == ref_other._minor_version ||
                        ref_other._minor_version == SD_ANY_MINOR_VERSION);
            }
        };

        struct SdEventgroupInfo
        {
            uint16_t _service_id;
            uint16_t _instance_id;
            uint8_t _major_version;
            uint32_t _ttl;
            uint16_t _eventgroup_id;
            uint8_t _counter = 0;
            bool has_multicast_events = false;

            SdEventgroupInfo() {};
            SdEventgroupInfo(uint16_t service_id,
                               uint16_t instance_id,
                               uint8_t major_version,
                               uint16_t eventgroup_id,
                               uint32_t ttl = SD_MAX_TTL) : _service_id(service_id),
                                                            _instance_id(instance_id),
                                                            _major_version(major_version),
                                                            _ttl(ttl),
                                                            _eventgroup_id(eventgroup_id) {};
        };

        struct SdEndpointInfo
        {
            IpAddr _ip;
            uint16_t _port;
            TransportKind _transport_kind = TransportKind::UDP;
            TcpConnection* _tcp_connection = nullptr;
            SdEndpointInfo() {};
            SdEndpointInfo(IpAddr ip, uint16_t port) : _ip(ip), _port(port) {};
            bool operator==(const SdEndpointInfo &ref_other) const
            {
                return _ip == ref_other._ip &&
                       _port == ref_other._port;
            }
        };

        struct SdSubscriberInfo
        {
            SdEndpointInfo _option;
            SdEndpointInfo _sender;
            bool operator==(const SdSubscriberInfo &ref_other) const
            {
                return _option == ref_other._option && _sender == ref_other._sender;
            }
            SdSubscriberInfo() {};
            SdSubscriberInfo(const SdEndpointInfo &ref_sender, const SdEndpointInfo &ref_option) : _sender(ref_sender), _option(ref_option) {}
        };

        // bounded event payload, no heap, access guarded by sd_service
        using event_payload = StaticVector<uint8_t, config::MAX_EVENT_PAYLOAD>;

        struct Event
        {
            uint16_t event_id = 0;
            uint16_t eventgroup_id = 0;
            bool on_change = false;
            TransportKind transport_kind = TransportKind::DEFAULT;
            event_payload payload;
        };

        struct Eventgroup {
          uint16_t eventgroup_id = 0;
          StaticVector<uint16_t, config::MAX_EVENTS_PER_EVENTGROUP> event_ids;
          bool cyclic_running = false;
          someIp::PoolPtr<TimerEvent> cyclic_timer; // periodic notify on the SD scheduler
        };

        typedef uint32_t ipv4;

    } // namespace sd
} // namespace someIP

#endif // SOMEIP_SD_FLAGS_HPP_
