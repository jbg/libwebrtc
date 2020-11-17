/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/emulated_turn_server.h"

#include <string>
#include <utility>

#include "api/packet_socket_factory.h"

namespace {

static const char kTestRealm[] = "example.org";
static const char kTestSoftware[] = "TestTurnServer";

// A wrapper class for copying data between an AsyncPacketSocket and a
// EmulatedEndpoint.
class AsyncPacketSocketWrapper
    : public rtc::AsyncPacketSocket,
      public webrtc::EmulatedNetworkReceiverInterface {
 public:
  AsyncPacketSocketWrapper(rtc::Thread* thread,
                           webrtc::EmulatedEndpoint* endpoint)
      : thread_(thread), endpoint_(endpoint) {
    port_ = endpoint_->BindReceiver(0, this).value();
  }
  rtc::SocketAddress GetLocalAddress() const override {
    return rtc::SocketAddress(endpoint_->GetPeerLocalAddress(), port_);
  }
  rtc::SocketAddress GetRemoteAddress() const override {
    return rtc::SocketAddress();
  }
  int Send(const void* pv,
           size_t cb,
           const rtc::PacketOptions& options) override {
    RTC_CHECK(false) << "TCP not implemented";
    return -1;
  }
  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options) override {
    // Copy from rtc::AsyncPacketSocket to EmulatedEndpoint.
    rtc::CopyOnWriteBuffer buf(reinterpret_cast<const char*>(pv), cb);
    endpoint_->SendPacket(GetLocalAddress(), addr, buf);
    return cb;
  }
  int Close() override { return 0; }

  rtc::AsyncPacketSocket::State GetState() const override {
    return rtc::AsyncPacketSocket::STATE_BOUND;
  }
  int GetOption(rtc::Socket::Option opt, int* value) override { return 0; }
  int SetOption(rtc::Socket::Option opt, int value) override { return 0; }
  int GetError() const override { return 0; }
  void SetError(int error) override {}
  void OnPacketReceived(webrtc::EmulatedIpPacket packet) override {
    // Copy from EmulatedEndpoint to rtc::AsyncPacketSocket.
    thread_->Invoke<void>(RTC_FROM_HERE, [this, &packet]() {
      SignalReadPacket(this, reinterpret_cast<const char*>(packet.cdata()),
                       packet.size(), packet.from, packet.arrival_time.ms());
    });
  }

 private:
  uint16_t port_;
  rtc::Thread* thread_;
  webrtc::EmulatedEndpoint* endpoint_;
};

struct PacketSocketFactoryWrapper : public rtc::PacketSocketFactory {
  explicit PacketSocketFactoryWrapper(
      webrtc::test::EmulatedTURNServer* turn_server)
      : turn_server_(turn_server) {}
  ~PacketSocketFactoryWrapper() override {}

  // This method is called from TurnServer when making a TURN ALLOCATION.
  // It will create a socket on the |peer_| endpoint.
  rtc::AsyncPacketSocket* CreateUdpSocket(const rtc::SocketAddress& address,
                                          uint16_t min_port,
                                          uint16_t max_port) override {
    return turn_server_->CreatePeerSocket();
  }

  rtc::AsyncPacketSocket* CreateServerTcpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override {
    return nullptr;
  }
  rtc::AsyncPacketSocket* CreateClientTcpSocket(
      const rtc::SocketAddress& local_address,
      const rtc::SocketAddress& remote_address,
      const rtc::ProxyInfo& proxy_info,
      const std::string& user_agent,
      const rtc::PacketSocketTcpOptions& tcp_options) override {
    return nullptr;
  }
  rtc::AsyncResolverInterface* CreateAsyncResolver() override {
    return nullptr;
  }

  webrtc::test::EmulatedTURNServer* turn_server_;
};

}  //  namespace

namespace webrtc {
namespace test {

EmulatedTURNServer::EmulatedTURNServer(std::unique_ptr<rtc::Thread> thread,
                                       EmulatedEndpoint* client,
                                       EmulatedEndpoint* peer)
    : thread_(std::move(thread)), client_(client), peer_(peer) {
  ice_config_.username = "keso";
  ice_config_.password = "keso";
  thread_->Invoke<void>(RTC_FROM_HERE, [=]() {
    turn_server_ = std::make_unique<cricket::TurnServer>(thread_.get());
    turn_server_->set_realm(kTestRealm);
    turn_server_->set_realm(kTestSoftware);
    turn_server_->set_auth_hook(this);

    auto client_socket = Wrap(client_);
    turn_server_->AddInternalSocket(client_socket, cricket::PROTO_UDP);
    turn_server_->SetExternalSocketFactory(new PacketSocketFactoryWrapper(this),
                                           rtc::SocketAddress());
    client_address_ = client_socket->GetLocalAddress();
    char buf[256];
    rtc::SimpleStringBuilder str(buf);
    str.AppendFormat("turn:%s?transport=udp",
                     client_address_.ToString().c_str());
    ice_config_.url = str.str();
  });
}

EmulatedTURNServer::~EmulatedTURNServer() {
  thread_->Invoke<void>(RTC_FROM_HERE, [=]() { turn_server_.reset(nullptr); });
}

rtc::AsyncPacketSocket* EmulatedTURNServer::Wrap(EmulatedEndpoint* endpoint) {
  return new AsyncPacketSocketWrapper(thread_.get(), endpoint);
}

}  // namespace test
}  // namespace webrtc
