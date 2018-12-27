/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_NETWORK_H_
#define TEST_PC_E2E_NETWORK_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/network.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/timestamp.h"
#include "rtc_base/asyncsocket.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/network_emulation_manager.h"

namespace webrtc {
namespace test {

// Network node, which doesn't add any extra delay or packet loss and just pass
// all incoming packets to the registered receiver.
class TransparentNetworkNode : public webrtc::NetworkNode {
 public:
  explicit TransparentNetworkNode(int id);
  ~TransparentNetworkNode() override;

  void Process(Timestamp cur_time) override;
  void SetReceiver(int endpoint_id, webrtc::NetworkNode* node) override;

 protected:
  void SendPacketInternal(
      std::unique_ptr<EmulatedNetworkPacket> packet) override;

 private:
  rtc::CriticalSection lock_;
  std::map<int, webrtc::NetworkNode*> routing_ RTC_GUARDED_BY(lock_);
  std::deque<std::unique_ptr<EmulatedNetworkPacket>> packets_
      RTC_GUARDED_BY(lock_);
};

// Network node, which behavior is based on NetworkBehaviorInterface.
class EmulatedNetworkNode : public webrtc::NetworkNode {
 public:
  EmulatedNetworkNode(
      int id,
      std::unique_ptr<NetworkBehaviorInterface> network_behavior);
  ~EmulatedNetworkNode() override;

  void Process(Timestamp cur_time) override;
  void SetReceiver(int endpoint_id, webrtc::NetworkNode* node) override;

 protected:
  void SendPacketInternal(
      std::unique_ptr<EmulatedNetworkPacket> packet) override;

 private:
  struct StoredPacket {
    StoredPacket(uint64_t id,
                 std::unique_ptr<EmulatedNetworkPacket> packet,
                 bool removed);
    ~StoredPacket();

    uint64_t id;
    std::unique_ptr<EmulatedNetworkPacket> packet;
    bool removed;
  };

  rtc::CriticalSection crit_sect_;
  size_t packet_overhead_ RTC_GUARDED_BY(crit_sect_);
  const std::unique_ptr<NetworkBehaviorInterface> network_behavior_
      RTC_GUARDED_BY(crit_sect_);
  std::map<uint64_t, webrtc::NetworkNode*> routing_ RTC_GUARDED_BY(crit_sect_);
  std::deque<std::unique_ptr<StoredPacket>> packets_ RTC_GUARDED_BY(crit_sect_);

  uint64_t next_packet_id_ RTC_GUARDED_BY(crit_sect_) = 1;
};

class EndpointNodeImpl : public webrtc::EndpointNode {
 public:
  EndpointNodeImpl(int endpoint_id,
                   webrtc::NetworkNode* entry_node,
                   webrtc::NetworkNode* exit_node);
  ~EndpointNodeImpl() override;

  void SendPacket(const rtc::SocketAddress& from,
                  const rtc::SocketAddress& to,
                  rtc::CopyOnWriteBuffer packet,
                  Timestamp sent_time) override;
  bool BindSocket(rtc::SocketAddress* local_addr,
                  webrtc::FakeNetworkSocket* socket) override;
  void UnbindSocket(uint16_t port) override;
  absl::optional<rtc::SocketAddress> GetPeerLocalAddress() const override;

 protected:
  webrtc::NetworkNode* GetEntryNode() override;
  webrtc::NetworkNode* GetExitNode() override;
  void SetConnectedEndpoint(webrtc::EndpointNode* endpoint) override;
  void SetNetworkThread(rtc::Thread* network_thread) override;

 private:
  // Routes packets in different sockets in the single endpoint basing on port.
  // Also assign port number to the newly registered sockets, if they haven't
  // own.
  class Router {
   public:
    Router();
    ~Router();

    // Registers socket in the routing table. Set socket port if it is missing.
    bool RegisterSocket(rtc::SocketAddress* local_addr,
                        webrtc::FakeNetworkSocket* socket);
    void UnregisterSocket(uint16_t port);
    // Returns socket, listening on port or nullptr if there is no such socket.
    webrtc::FakeNetworkSocket* GetSocket(uint16_t port);

   private:
    static constexpr uint16_t kFirstEphemeralPort = 49152;

    rtc::CriticalSection lock_;
    uint16_t next_port_ RTC_GUARDED_BY(lock_);
    std::map<uint16_t, webrtc::FakeNetworkSocket*> port_to_socket_
        RTC_GUARDED_BY(lock_);
  };

  class ProxyNode : public webrtc::NetworkNode {
   public:
    explicit ProxyNode(EndpointNodeImpl* endpoint);
    ~ProxyNode() override;

    void Process(Timestamp cur_time) override {}
    void SetReceiver(int endpoint_id, webrtc::NetworkNode* node) override {}

   protected:
    void SendPacketInternal(
        std::unique_ptr<EmulatedNetworkPacket> packet) override;

   private:
    EndpointNodeImpl* const endpoint_;
  };

  webrtc::EndpointNode* connected_endpoint_;

  webrtc::NetworkNode* const entry_node_;
  webrtc::NetworkNode* const exit_node_;
  std::unique_ptr<ProxyNode> proxy_node_;
  std::unique_ptr<Router> router_;

  // Will be used to deliver packets to socket.
  rtc::Thread* network_thread_;

  // Peer's local IP address for this endpoint network node.
  absl::optional<rtc::SocketAddress> peer_local_addr_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_NETWORK_H_
