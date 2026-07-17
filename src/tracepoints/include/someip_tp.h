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

// LTTng-UST provider, tracepoints become no-ops when tracing is off
#ifdef SOMEIP_ENABLE_TRACING

#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER someip

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "./someip_tp.h"

#if !defined(_SOMEIP_TP_H) || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define _SOMEIP_TP_H

#include <lttng/tracepoint.h>

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    app_send_request,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
        lttng_ust_field_integer(int, dest_port, dest_port_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    api_send_request,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg,
        uint16_t, session_id_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
        lttng_ust_field_integer(int, dest_port, dest_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    api_send_response,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg,
        uint16_t, session_id_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
        lttng_ust_field_integer(int, dest_port, dest_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    app_response_received,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg,
        uint16_t, session_id_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
        lttng_ust_field_integer(int, dest_port, dest_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    app_received,
    LTTNG_UST_TP_ARGS(
        char *, source_ip_arg,
        uint16_t, source_port_arg,
        uint16_t, session_id_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(source_ip, source_ip_arg)
        lttng_ust_field_integer(int, source_port, source_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    echo_received,
    LTTNG_UST_TP_ARGS(
        char *, source_ip_arg,
        uint16_t, source_port_arg,
        uint16_t, session_id_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(source_ip, source_ip_arg)
        lttng_ust_field_integer(int, source_port, source_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    api_fire_and_forget,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
        lttng_ust_field_integer(int, dest_port_arg, dest_port_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    lwip_udp_send,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg,
        uint16_t, session_id_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
        lttng_ust_field_integer(int, dest_port_arg, dest_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    lwip_udp_receive,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg,
        uint16_t, session_id_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(source_ip, dest_ip_arg)
        lttng_ust_field_integer(int, source_port_arg, dest_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    general,
    LTTNG_UST_TP_ARGS(
        char *, info_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(info, info_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    my_first_tracepoint,
    LTTNG_UST_TP_ARGS(
        int, my_integer_arg,
        char *, my_string_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(my_string_field, my_string_arg)
        lttng_ust_field_integer(int, my_integer_field, my_integer_arg)
    )
)


LTTNG_UST_TRACEPOINT_EVENT(
    someip, 
    lwip_tcp_send, 
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg, 
        uint16_t, dest_port_arg, 
        uint16_t, session_id_arg
    ), 
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
        lttng_ust_field_integer(int, dest_port, dest_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip, 
    lwip_tcp_receive, 
    LTTNG_UST_TP_ARGS(
        char *, source_ip_arg, 
        uint16_t, source_port_arg, 
        uint16_t, session_id_arg
    ), 
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(source_ip, source_ip_arg)
        lttng_ust_field_integer(int, source_port, source_port_arg)
        lttng_ust_field_integer(int, session_id, session_id_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
   someip,
   lwip_tcp_connect,
   LTTNG_UST_TP_ARGS(
       char *, dest_ip_arg,
       uint16_t, dest_port_arg
   ),
  LTTNG_UST_TP_FIELDS(
       lttng_ust_field_string(dest_ip, dest_ip_arg)
       lttng_ust_field_integer(int, dest_port, dest_port_arg)
   )
)

// Service Discovery Message Events

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    sd_server_started,
    LTTNG_UST_TP_ARGS(
  ),
    LTTNG_UST_TP_FIELDS(
  )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    sd_client_started,
    LTTNG_UST_TP_ARGS(),
    LTTNG_UST_TP_FIELDS())

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    sd_message_sent,
    LTTNG_UST_TP_ARGS(
        char *, dest_ip_arg,
        uint16_t, dest_port_arg,
        uint32_t, session_id_arg,
        uint8_t, entry_count_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(dest_ip, dest_ip_arg)
            lttng_ust_field_integer(int, dest_port, dest_port_arg)
                lttng_ust_field_integer(int, session_id, session_id_arg)
                    lttng_ust_field_integer(int, entry_count, entry_count_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    sd_message_received,
    LTTNG_UST_TP_ARGS(
        char *, source_ip_arg,
        uint16_t, source_port_arg,
        uint32_t, session_id_arg,
        uint8_t, entry_count_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(source_ip, source_ip_arg)
            lttng_ust_field_integer(int, source_port, source_port_arg)
                lttng_ust_field_integer(int, session_id, session_id_arg)
                    lttng_ust_field_integer(int, entry_count, entry_count_arg)))

// Service Lifecycle Events
LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    offer_service_sent,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg,
        uint32_t, ttl_arg,
        uint16_t, port_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)
                        lttng_ust_field_integer(int, ttl, ttl_arg)
                            lttng_ust_field_integer(int, port, port_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    find_service_sent,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    package_sent,
    LTTNG_UST_TP_ARGS(
        uint16_t, payload_size_arg,
        const char *, ip_arg,
        uint16_t, port_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint16_t, payload_size, payload_size_arg)
        lttng_ust_field_string(ip, ip_arg)
        lttng_ust_field_integer(uint16_t, port, port_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    package_received,
    LTTNG_UST_TP_ARGS(
        uint16_t, payload_size_arg,
        const char *, ip_arg,
        uint16_t, port_arg
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint16_t, payload_size, payload_size_arg)
        lttng_ust_field_string(ip, ip_arg)
        lttng_ust_field_integer(uint16_t, port, port_arg)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    find_service_main_phase_started,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    find_service_main_phase_checked,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    find_service_stop_phase,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    find_service_received,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    service_stop_offer_sent,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    offer_service_received,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg,
        uint32_t, ttl_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)
                        lttng_ust_field_integer(int, ttl, ttl_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    offer_services_sent,
    LTTNG_UST_TP_ARGS(
        int, service_count_arg,
        uint16_t, sender_session_id_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_count, service_count_arg)
            lttng_ust_field_integer(int, sender_session_id, sender_session_id_arg)))

LTTNG_UST_TRACEPOINT_EVENT(someip, start_offering_services, LTTNG_UST_TP_ARGS(), LTTNG_UST_TP_FIELDS())

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    eventgroup_subscribe_sent,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint16_t, eventgroup_id_arg,
        uint32_t, ttl_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, eventgroup_id, eventgroup_id_arg)
                        lttng_ust_field_integer(int, ttl, ttl_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    eventgroup_subscribe_received,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint16_t, eventgroup_id_arg,
        uint32_t, ttl_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, eventgroup_id, eventgroup_id_arg)
                        lttng_ust_field_integer(int, ttl, ttl_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    subscribe_ack_sent,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint16_t, eventgroup_id_arg,
        uint32_t, ttl_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, eventgroup_id, eventgroup_id_arg)
                        lttng_ust_field_integer(int, ttl, ttl_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    subscribe_ack_received,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint16_t, eventgroup_id_arg,
        uint32_t, ttl_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, eventgroup_id, eventgroup_id_arg)
                        lttng_ust_field_integer(int, ttl, ttl_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    find_service_initial_phase_started,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg,
        uint32_t, period_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)
                        lttng_ust_field_integer(int, period, period_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    find_service_repetition_phase_started,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint32_t, minor_version_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    offer_service_repetition_phase_started,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint16_t, minor_version_arg,
        uint16_t, port_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)
                        lttng_ust_field_integer(int, port, port_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    offer_service_main_phase_started,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint16_t, minor_version_arg,
        uint16_t, port_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)
                        lttng_ust_field_integer(int, port, port_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    offer_service_initial_phase_started,
    LTTNG_UST_TP_ARGS(
        uint16_t, service_id_arg,
        uint16_t, instance_id_arg,
        uint8_t, major_version_arg,
        uint16_t, minor_version_arg,
        uint16_t, port_arg,
        uint32_t, period_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, service_id, service_id_arg)
            lttng_ust_field_integer(int, instance_id, instance_id_arg)
                lttng_ust_field_integer(int, major_version, major_version_arg)
                    lttng_ust_field_integer(int, minor_version, minor_version_arg)
                        lttng_ust_field_integer(int, port, port_arg)
                            lttng_ust_field_integer(int, period, period_arg)))

// Manager Events
LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    manager_started,
    LTTNG_UST_TP_ARGS(
        char *, local_ip_arg,
        uint16_t, local_port_arg,
        int, service_count_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(local_ip, local_ip_arg)
            lttng_ust_field_integer(int, local_port, local_port_arg)
                lttng_ust_field_integer(int, service_count, service_count_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    manager_stopped,
    LTTNG_UST_TP_ARGS(
        char *, local_ip_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(local_ip, local_ip_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    multicast_joined,
    LTTNG_UST_TP_ARGS(
        char *, multicast_ip_arg,
        char *, local_ip_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(multicast_ip, multicast_ip_arg)
            lttng_ust_field_string(local_ip, local_ip_arg)))

// Timer Events
LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    timer_started,
    LTTNG_UST_TP_ARGS(
        char *, timer_name_arg,
        uint32_t, interval_ms_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(timer_name, timer_name_arg)
            lttng_ust_field_integer(int, interval_ms, interval_ms_arg)))

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    timer_expired,
    LTTNG_UST_TP_ARGS(
        char *, timer_name_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(timer_name, timer_name_arg)))

// Error Events
LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    error_occurred,
    LTTNG_UST_TP_ARGS(
        char *, error_msg_arg,
        int, error_code_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(error_msg, error_msg_arg)
            lttng_ust_field_integer(int, error_code, error_code_arg)))

// General debug event
LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    debug_info,
    LTTNG_UST_TP_ARGS(
        char *, info_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(info, info_arg)))

// Simple test tracepoints
LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    test_event,
    LTTNG_UST_TP_ARGS(
        const char *, message_arg),
    LTTNG_UST_TP_FIELDS(lttng_ust_field_string(message, message_arg))
)

// PoolPtr provenance and leak tracing
LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    pool_alloc,
    LTTNG_UST_TP_ARGS(
        uint16_t, pool_id_arg,
        const char *, alloc_site_arg,
        uint32_t, live_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, pool_id, pool_id_arg)
        lttng_ust_field_string(alloc_site, alloc_site_arg)
        lttng_ust_field_integer(int, live, live_arg))
)

LTTNG_UST_TRACEPOINT_EVENT(
    someip,
    pool_free,
    LTTNG_UST_TP_ARGS(
        uint16_t, pool_id_arg,
        const char *, alloc_site_arg,
        uint32_t, live_arg),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(int, pool_id, pool_id_arg)
        lttng_ust_field_string(alloc_site, alloc_site_arg)
        lttng_ust_field_integer(int, live, live_arg))
)

#include <lttng/tracepoint-event.h>
#endif /* _SOMEIP_TP_H */

#else /* !SOMEIP_ENABLE_TRACING */

/* tracing disabled, call sites expand to nothing */
#ifndef SOMEIP_TP_NOOP_DEFINED
#define SOMEIP_TP_NOOP_DEFINED
#define lttng_ust_tracepoint(...) do { } while (0)
#endif

#endif /* SOMEIP_ENABLE_TRACING */
