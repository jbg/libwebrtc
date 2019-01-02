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
#include <string>
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
struct EmulatedIpPacket {
 public:
  EmulatedIpPacket(const rtc::SocketAddress& from,
                   const rtc::SocketAddress to,
                   std::string dest_endpoint_id,
                   rtc::CopyOnWriteBuffer data,
                   Timestamp sent_time);
  ~EmulatedIpPacket();

  rtc::SocketAddress from;
  rtc::SocketAddress to;
  std::string dest_endpoint_id;
  rtc::CopyOnWriteBuffer data;
  Timestamp sent_time;
};

// This API is in development. It can be changed/removed without notice.
class NetworkReceiverInterface {
 public:
  explicit NetworkReceiverInterface(std::string id) : id_(id) {}
  virtual ~NetworkReceiverInterface() = default;

  // Should be used only for logging. No unique guarantees provided.
  std::string GetId() const { return id_; }
  void DeliverPacket(std::unique_ptr<EmulatedIpPacket> packet) {
    DeliverPacketInternal(std::move(packet));
  }

 protected:
  virtual void DeliverPacketInternal(
      std::unique_ptr<EmulatedIpPacket> packet) = 0;

 private:
  // Node id for logging purpose.
  std::string const id_;
};

// This API is in development. It can be changed/removed without notice.
// Represents node in the emulated network. Nodes can be connected with each
// other to form different networks with different behavior.
class NetworkNode : public NetworkReceiverInterface {
 public:
  explicit NetworkNode(std::string id) : NetworkReceiverInterface(id) {}
  ~NetworkNode() override = default;

  virtual void Process(Timestamp cur_time) = 0;
  virtual void SetReceiver(std::string dest_endpoint_id,
                           NetworkReceiverInterface* receiver) = 0;
};

// This API is in development. It can be changed/removed without notice.
// Represents a socket, which will operate with emulated network.
class FakeNetworkSocket : public rtc::AsyncSocket {
 public:
  explicit FakeNetworkSocket(std::string id) : id_(id) {}
  ~FakeNetworkSocket() override = default;

  // Should be used only for logging. No unique guarantees provided.
  std::string GetId() const { return id_; }
  virtual void DeliverPacket(rtc::CopyOnWriteBuffer packet,
                             const rtc::SocketAddress& source_addr) = 0;

 private:
  // Node id for logging purpose.
  std::string const id_;
};

// This API is in development. It can be changed/removed without notice.
// Represents an entry point to the simulated network. Is used to send and
// receive data to/from the network.
class EndpointNode : public NetworkReceiverInterface {
 public:
  explicit EndpointNode(std::string id) : NetworkReceiverInterface(id) {}
  ~EndpointNode() override = default;

  virtual void SendPacket(const rtc::SocketAddress& from,
                          const rtc::SocketAddress& to,
                          rtc::CopyOnWriteBuffer packet,
                          Timestamp sent_time) = 0;
  virtual bool BindSocket(rtc::SocketAddress* local_addr,
                          FakeNetworkSocket* socket) = 0;
  virtual void UnbindSocket(uint16_t port) = 0;
  // Returns peer's local IP address for this endpoint network node.
  virtual absl::optional<rtc::SocketAddress> GetPeerLocalAddress() const = 0;
  // Sets peer's local IP address for this endpoint network node.
  virtual void SetPeerLocalAddress(
      absl::optional<rtc::SocketAddress> address) = 0;

 protected:
  friend NetworkEmulationManager;

  // Returns network node, that should be used to send data.
  virtual NetworkNode* GetSendNode() = 0;
  // Returns network node, that should be used to receive data.
  virtual NetworkNode* GetReceiveNode() = 0;
  // Sets endpoint, to which this one is connected
  virtual void SetConnectedEndpoint(EndpointNode* endpoint) = 0;
  // Sets thread on which received packets should proceed to the socket.
  virtual void SetNetworkThread(rtc::Thread* network_thread) = 0;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_H_
