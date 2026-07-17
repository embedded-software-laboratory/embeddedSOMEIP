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

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "someIp/ESomeIp.hpp"
#include "someIp/enums/ReturnCode.hpp"
#include "someIp/net/IpAddress.hpp"
#include "someIp/structs/Method.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/Service.hpp"

#include "mocks/MockNetworkRouter.hpp"
#include "mocks/MockUdpTransport.hpp"
#include "utils/MockHarness.hpp"

using namespace someIp;

namespace {

constexpr uint16_t SERVER_PORT = 8000;
constexpr uint16_t CLIENT_PORT = 8001;
constexpr uint16_t KNOWN_SERVICE = 0x1234;
constexpr uint16_t KNOWN_METHOD = 1;

// SOME/IP header byte offset [PRS_SOMEIP_00042]
constexpr size_t RETURN_CODE_OFFSET = 15;

class ErrorResponseTests : public test::MockHarnessTest {
protected:
  // the server answers to the source endpoint, so send and listen on one transport
  class ClientPeer {
  public:
    ClientPeer(const char *ip, uint16_t port) : m_ip(test::make_ip(ip)), m_port(port) {
      m_transport.init(IpEndpoint(m_ip.v4, 0));
      m_transport.set_rx_callback([this](RxPacket &&p) {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_bytes.empty()) {
          m_bytes.assign(p.buffer.data(), p.buffer.data() + p.buffer.size());
        }
        m_got = true;
      });
      m_transport.open_listen_port(port);
    }

    void send_request(uint16_t serviceId, uint16_t methodId, const IpEndpoint &server) {
      Package req(serviceId, methodId, "PING", IpAddr(server.v4), server.port);
      req.header._message_type = MessageType::REQUEST;
      PacketBuffer wire = req.serialize();
      m_transport.send_to(server, make_vector_buffer(wire.take_bytes()), m_port);
    }

    bool wait() { return test::wait_for(m_got); }

    std::vector<uint8_t> bytes() {
      std::lock_guard<std::mutex> lk(m_mtx);
      return m_bytes;
    }

  private:
    test::MockUdpTransport m_transport;
    IpAddr m_ip;
    uint16_t m_port;
    std::atomic<bool> m_got{false};
    std::mutex m_mtx;
    std::vector<uint8_t> m_bytes;
  };

  // a server offering KNOWN_SERVICE / KNOWN_METHOD and nothing else
  void start_server(ESomeIp &server) {
    server.init(test::make_ip("192.168.1.10"));
    server.set_transport_mode(TransportKind::UDP);
    server.listen_to_port(SERVER_PORT);

    Callback method_cb([&server](PackageRx &&p) { server.send_response(std::move(p), "PONG"); });
    Service svc(static_cast<uint16_t>(KNOWN_SERVICE));
    svc.register_method(Method(static_cast<uint16_t>(KNOWN_METHOD), std::move(method_cb)));
    server.register_service(std::move(svc));
  }

  IpEndpoint server_endpoint() const {
    return IpEndpoint(test::make_ip("192.168.1.10").v4, SERVER_PORT);
  }
};

} // namespace

TEST_F(ErrorResponseTests, UnknownServiceGetsErrorResponse) {
  ESomeIp server(test::mock_populator());
  start_server(server);

  ClientPeer peer("192.168.1.20", CLIENT_PORT);
  peer.send_request(0x9999 /* never offered */, KNOWN_METHOD, server_endpoint());

  ASSERT_TRUE(peer.wait()) << "server never answered an unknown-service request";
  auto bytes = peer.bytes();
  ASSERT_GE(bytes.size(), 16u);
  EXPECT_EQ(bytes[RETURN_CODE_OFFSET], static_cast<uint8_t>(ReturnCode::E_UNKNOWN_SERVICE));
}

TEST_F(ErrorResponseTests, UnknownMethodGetsErrorResponse) {
  ESomeIp server(test::mock_populator());
  start_server(server);

  ClientPeer peer("192.168.1.20", CLIENT_PORT);
  peer.send_request(KNOWN_SERVICE, 0x77 /* never registered */, server_endpoint());

  ASSERT_TRUE(peer.wait()) << "server never answered an unknown-method request";
  auto bytes = peer.bytes();
  ASSERT_GE(bytes.size(), 16u);
  EXPECT_EQ(bytes[RETURN_CODE_OFFSET], static_cast<uint8_t>(ReturnCode::E_UNKNOWN_METHOD));
}

TEST_F(ErrorResponseTests, KnownMethodGetsOkResponse) {
  ESomeIp server(test::mock_populator());
  start_server(server);

  ClientPeer peer("192.168.1.20", CLIENT_PORT);
  peer.send_request(KNOWN_SERVICE, KNOWN_METHOD, server_endpoint());

  ASSERT_TRUE(peer.wait()) << "server never answered a valid request";
  auto bytes = peer.bytes();
  ASSERT_GE(bytes.size(), 16u);
  EXPECT_EQ(bytes[RETURN_CODE_OFFSET], static_cast<uint8_t>(ReturnCode::E_OK));
}
