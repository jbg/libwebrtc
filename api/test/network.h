/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_H_
#define API_TEST_NETWORK_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "rtc_base/asyncsocket.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"

namespace webrtc {

class NetworkEmulationManager;

// This API is in development. It can be changed/removed without notice.
struct EmulatedNetworkPacket {
 public:
  EmulatedNetworkPacket(const rtc::SocketAddress& from,
                        const rtc::SocketAddress to,
                        int dest_endpoint_id,
                        rtc::CopyOnWriteBuffer data,
                        Timestamp sent_time,
                        std::vector<int> trace);
  ~EmulatedNetworkPacket();

  rtc::SocketAddress from;
  rtc::SocketAddress to;
  int dest_endpoint_id;
  rtc::CopyOnWriteBuffer data;
  Timestamp sent_time;
  // Constains ids of nodes, through which packet went through.
  std::vector<int> trace;
};

// This API is in development. It can be changed/removed without notice.
// Represents node in the emulated network. Nodes can be connected with each
// other to form different networks with different behavior.
class NetworkNode {
 public:
  explicit NetworkNode(int id) : id_(id) {}
  virtual ~NetworkNode() = default;

  // Should be used only for logging. No unique guarantees provided.
  int GetId() const { return id_; }
  void SendPacket(std::unique_ptr<EmulatedNetworkPacket> packet) {
    packet->trace.push_back(id_);
    SendPacketInternal(std::move(packet));
  }
  virtual void Process(Timestamp cur_time) = 0;
  virtual void SetReceiver(int endpoint_id, NetworkNode* node) = 0;

 protected:
  virtual void SendPacketInternal(
      std::unique_ptr<EmulatedNetworkPacket> packet) = 0;

 private:
  // Node id for logging purpose.
  int const id_;
};

// This API is in development. It can be changed/removed without notice.
// Represents a socket, which will operate with emulated network.
class FakeNetworkSocket : public rtc::AsyncSocket {
 public:
  explicit FakeNetworkSocket(int id) : id_(id) {}
  ~FakeNetworkSocket() override = default;

  // Should be used only for logging. No unique guarantees provided.
  int GetId() const { return id_; }
  virtual void DeliverPacket(rtc::CopyOnWriteBuffer packet,
                             const rtc::SocketAddress& source_addr) = 0;

 private:
  // Node id for logging purpose.
  int const id_;
};

// This API is in development. It can be changed/removed without notice.
// Represents an entry point to the simulated network. Is used to send and
// receive data to/from the network.
class EndpointNode {
 public:
  explicit EndpointNode(int id) : id_(id) {}
  virtual ~EndpointNode() = default;

  // Should be used only for logging. No unique guarantees provided.
  int GetId() const { return id_; }
  virtual void SendPacket(const rtc::SocketAddress& from,
                          const rtc::SocketAddress& to,
                          rtc::CopyOnWriteBuffer packet,
                          Timestamp sent_time) = 0;
  virtual bool BindSocket(rtc::SocketAddress* local_addr,
                          FakeNetworkSocket* socket) = 0;
  virtual void UnbindSocket(uint16_t port) = 0;
  // Returns peer's local IP address for this endpoint network node.
  virtual absl::optional<rtc::SocketAddress> GetPeerLocalAddress() const = 0;

 protected:
  friend NetworkEmulationManager;

  // Returns network node, that should be used to send data.
  virtual NetworkNode* GetEntryNode() = 0;
  // Returns network node, that should be used to receive data.
  virtual NetworkNode* GetExitNode() = 0;
  // Sets endpoint, to which this one is connected
  virtual void SetConnectedEndpoint(EndpointNode* endpoint) = 0;
  // Sets thread on which received packets should proceed to the socket.
  virtual void SetNetworkThread(rtc::Thread* network_thread) = 0;

 private:
  // Node id for logging purpose.
  int const id_;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_H_
