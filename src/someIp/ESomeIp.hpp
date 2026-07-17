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

#ifndef SOMEIP_ESOMEIP_HPP
#define SOMEIP_ESOMEIP_HPP

#include <string>
#include "someIp/utils/PoolPtr.hpp"
#include <string_view>
#include <unordered_map>
#include <functional>
#include <limits>
#include <memory>
#include <optional>

#include "structs/Package.hpp"
#include "ThreadPool.hpp"
#include "config/config.hpp"
#include "someIp/Types.hpp"
#include "someIp/handler/ServiceHandler.hpp"
#include "someIp/handler/RxContext.hpp"
#ifdef SOMEIP_PLATFORM_STM32
#include "someIp/communication/udp/UdpDriver.hpp"
#endif
#include "someIp/transport-protocol/Segmenter.hpp"
#include "someIp/transport-protocol/utils/TxSessionManager.hpp"
#include "someIp/transport-protocol/config/Tp_Config.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/transport-protocol/Reassembler.hpp"
#include "someIp/config/communication_config.hpp"
#ifdef SOMEIP_PLATFORM_STM32
#include "someIp/communication/tcp/TcpDriver.hpp"
#endif
// host gets an lwIP-free TcpConnection stub, pointer stays opaque
#include "someIp/communication/tcp/TcpConnection.hpp"
#include "someIp/communication/ITransport.hpp"
#include "someIp/communication/TransportRegistry.hpp"
#include "someIp/os/Os.hpp"
#include "someIp/utils/SessionTable.hpp"

#include "someIp/service_discovery/SdManager.hpp"


namespace someIp {

class ESomeIp {
    private:
    TransportKind m_transportMode = TransportKind::UDP_TP;

    // source port for outgoing app traffic, set by listen_to_port
    uint16_t m_local_port = config::SD_PORT;

    // per-instance init guard so each eSomeIP initializes independently
    bool m_initialized = false;

    // owned per-eSomeIP service registry, not a singleton
    ServiceHandler m_serviceHandler;

#ifdef SOMEIP_PLATFORM_STM32
    // singleton TCP driver, retained for the native-connection TCP methods (lwIP only)
    TcpDriver* const m_ptr_tcpDriver = TcpDriver::get_instance();
#endif

    // owned TP transmit state, no separate TP API object
    tp::TxSessionManager m_tpTxSessions;
    tp::Segmenter m_tpSegmenter;
    const tp::ConfigType* m_ptr_tpConfig = &tp::TP_Config;

    // owned per-eSomeIP TP RX reassembler
    tp::Reassembler m_reassembler;

    // timer scheduler for reassembler RX-stall timeouts, destroyed before reassembler_
    TimerScheduler m_scheduler;

    // pluggable transport seam, declared before threadPool which references it
    TransportRegistry m_transports;

    someIp::sd::SdManager* m_ptr_sd_manager;

    // composite response-match key on full request identity + source endpoint (M-11)
    struct ResponseKey {
      uint16_t service = 0;
      uint16_t method = 0;
      uint16_t client = 0;
      uint16_t session = 0;
      IpAddr src{};
      uint16_t src_port = 0;

      bool operator==(const ResponseKey &ref_o) const {
        return service == ref_o.service && method == ref_o.method && client == ref_o.client &&
               session == ref_o.session && src_port == ref_o.src_port && src == ref_o.src;
      }
    };

    // bounded request->response callback table, slots reclaimed on consume
    struct ResponseSlot {
      bool active = false;
      uint16_t id = 0;       // session id, kept for logging
      ResponseKey key{};
      Callback cb;

      // SessionTable key hooks
      bool matches(const ResponseKey &ref_k) const { return key == ref_k; }
      void set_key(const ResponseKey &ref_k) { key = ref_k; id = ref_k.session; }
    };
    static SessionTable<ResponseSlot, config::MAX_OUTSTANDING_REQUESTS, ResponseKey> m_responseTable;

    // do not use directly, use get_new_session_id()
    static uint16_t m_next_session_id;

    // thread-safe session id, session id 0 is reserved per SOME/IP spec
    static uint16_t get_new_session_id();

    someIp::ThreadPool m_threadPool;

    RxContext m_rx_context;

    // SD manager owned when enable_sd() is called, destroyed before threadPool
    std::optional<sd::SdManager> m_owned_sd_manager;

    const uint16_t m_CLIENT_ID = config::CLIENT_ID;

    // transport RX sink, reconstructs a PackageRx and enqueues it
    void on_rx(RxPacket &&ref_pkt);

    void send_notifications(uint16_t serviceId, uint16_t eventId, const uint8_t *ptr_payload, size_t payloadLen, const sd::SubscriberVec &ref_subscribers, TransportKind transportKind = TransportKind::DEFAULT);

    // push current field values to a fresh subscriber (SD_INITIAL_NOTIFY_ON_SUBSCRIBE)
    void notify_subscriber_initial(const sd::SdServiceInfo &ref_info, uint16_t eventgroupId, const sd::SdSubscriberInfo &ref_subscriber);

    // segment (if needed) and transmit a serialized SOME/IP message over UDP-TP
    ReturnCode tp_transmit(IpAddr &ref_destAddr, uint16_t destPort, PacketBuffer &&ref_buffer);

    // null connection policy, LazyClient opens a POSIX client and lwIP rejects
    enum class TcpNullPolicy { Error, LazyClient, AllowNull };

    // single place the lwIP vs POSIX TCP backend difference lives
    bool send_over_tcp(PacketBuffer &&ref_buf, const IpAddr &ref_destIp, uint16_t destPort,
                       TcpConnection *ptr_tcpConn, TcpNullPolicy nullPolicy);

    public:

    void set_transport_mode(TransportKind t) { this->m_transportMode = t; }
    TransportKind get_transport_mode() {return this->m_transportMode; }

    // initiate a TCP connection to a remote server
    someIp::TcpConnection* connect_to_tcp_server(const IpAddr& ref_remote_ip, uint16_t remote_port);

    // check if a TCP connection to a remote server exists
    bool is_tcp_connected(const IpAddr& ref_remote_ip, uint16_t remote_port);


    ESomeIp();

    // construct with a caller-supplied transport populator
    using TransportPopulator = std::function<void(TransportRegistry &)>;
    explicit ESomeIp(TransportPopulator populate);
    ~ESomeIp();

    // start network drivers and worker threads
    void init(IpAddr local_ip);

    // open a UDP listen port, returns the actual bound port
    uint16_t listen_to_port(int port);

    // open a TCP port for listening
    someIp::TcpConnection* listen_to_tcp_port(int port);

    // backend-neutral TCP listener, returns true on success
    bool open_tcp_listener(uint16_t port);


    // send a SOME/IP message without expecting a response
    void fire_and_forget(Package &&ref_package, TransportKind transportKind = TransportKind::DEFAULT, TcpConnection* ptr_tcpConn = nullptr);

    // send a REQUEST and register a callback for the RESPONSE
    void request_and_response(Package &&ref_package, Callback callback, TransportKind transportKind = TransportKind::DEFAULT, TcpConnection* ptr_tcpConn = nullptr);

    // send a RESPONSE reusing the request's session id
    void send_response(PackageRx &&ref_request, std::string_view payload);

    // register a handler for incoming notifications or remote requests
    void register_event_handler(uint16_t serviceId, uint16_t eventId, Callback callback);

    // remove a previously registered event handler
    void deregister_event_handler(uint16_t serviceId, uint16_t eventId);

    // retrieve the current payload of an event
    sd::event_payload get_event_payload(sd::SdServiceInfo info, uint16_t eventId);

    // update local event payload and trigger notifications
    void set_event_payload(sd::SdServiceInfo info, uint16_t eventId, std::string_view payload);

    // periodically send notifications for an eventgroup
    void start_cyclic_update(sd::SdServiceInfo info, uint16_t eventgroupId, uint16_t intervalTime);

    // stop periodic notifications for an eventgroup
    void stop_cyclic_update(sd::SdServiceInfo info, uint16_t eventgroupId);

    // notify all subscribers of an event
    void notify_event(sd::SdServiceInfo service, uint16_t eventId);

    // notify all subscribers of an eventgroup
    void notify_eventgroup(sd::SdServiceInfo service, uint16_t eventgroupId);

    // takes ownership, false if the id exists or the registry is full
    bool register_service(Service newService);

    // returns a non-owning borrow, nullptr if the registry is full
    Service *add_service(uint16_t serviceId);

    // TODO add SD to this
    void deregister_service(uint16_t serviceId);

    // borrowed pointer into the handler store, nullptr if unknown
    Service *get_service(uint16_t serviceId);

    // owns the sd_manager, opens the SD port, joins SD multicast
    void enable_sd(IpAddr local_ip,
                   const sd::ServiceVec &ref_services = {});

    // start offering a local service over SD
    bool offer_service(someIp::PoolPtr<sd::SdService> sprt_service,
                       const IpAddr &ref_src_ip,
                       uint16_t src_port,
                       sd::OptionVec options = {});

    // stop offering a single service
    bool stop_offer_service(sd::SdServiceInfo service_info);

    // stop offering all services
    void stop_all_services();

    // register a local service managed by SD
    void register_sd_service(someIp::PoolPtr<sd::SdService> sprt_service);

    // register a client-side service for discovery
    void register_client_service(someIp::PoolPtr<sd::SdClientService> sprt_client);

    // send a FIND_SERVICE request
    bool send_find_service(const sd::SdServiceInfo &ref_service_info,
                           someIp::PoolPtr<sd::ConfigurationOption> sprt_config_option = nullptr);

    // resolve a service, local or remote
    const someIp::PoolPtr<sd::SdService> find_service(const sd::SdServiceInfo &ref_service_info);

    // subscribe to an eventgroup
    void subscribe(const sd::SdEventgroupInfo &ref_eventgroup,
                   sd::OptionVec options = {});

    // unsubscribe from all eventgroups
    void unsubscribe_all();

    // query the SD stop flag
    bool get_sd_stop_flag();

    // join a UDP multicast group via the active UDP transport
    void join_multicast_group(IpAddr multicast_ip){
      if(auto* ptr_udp = m_transports.get(TransportKind::UDP)){
        ptr_udp->join_multicast_group(IpEndpoint(multicast_ip.to_u32(), 0));
      }
    }

    // low-level send of a SOME/IP package
    bool send_package(Package &&ref_p, TransportKind transportKind = TransportKind::DEFAULT, TcpConnection* ptr_tcpConn = nullptr);

    // send a package specifically via TCP
    bool send_tcp_package(Package &&ref_p, TcpConnection* ptr_tcpConn);

    // find the first established TCP connection, for debugging/tests
    someIp::TcpConnection* get_first_established_peer_connection();


    static ResponseKey make_response_key(const Header &ref_hdr, const IpAddr &ref_src, uint16_t src_port);

    // store a response callback keyed on request identity + source endpoint (M-11)
    static bool store_response_callback(const Header &ref_hdr, const IpAddr &ref_src, uint16_t src_port, Callback cb);

    // find and remove the response callback, moving it into out
    static bool take_response_callback(const Header &ref_hdr, const IpAddr &ref_src, uint16_t src_port, Callback &ref_out);

    // RX intake, single entry point for every received package
    void add_to_queue(PackageRx &&ref_p);

    // this eSomeIP's TP RX reassembler (used by the dispatcher's TpHandler)
    tp::Reassembler *reassembler() { return &m_reassembler; }

    // this eSomeIP's service registry (used by the dispatcher's RxHandler)
    IServiceHandler *service_handler() { return &m_serviceHandler; }
  };
}

#endif // SOMEIP_ESOMEIP_HPP
