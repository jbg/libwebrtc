/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/network.h"

#include <limits>
#include <memory>

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

TransparentNetworkNode::TransparentNetworkNode(int id)
    : webrtc::NetworkNode(id) {}

TransparentNetworkNode::~TransparentNetworkNode() = default;

void TransparentNetworkNode::SendPacketInternal(
    std::unique_ptr<EmulatedIpPacket> packet) {
  rtc::CritScope crit(&lock_);
  if (routing_.find(packet->dest_endpoint_id) == routing_.end()) {
    RTC_LOG(INFO) << "PACKET DROPPED: No route for endpoint "
                  << packet->dest_endpoint_id << " in node " << GetId();
  }
  packets_.emplace_back(std::move(packet));
}

void TransparentNetworkNode::Process(Timestamp cur_time) {
  std::vector<std::unique_ptr<EmulatedIpPacket>> to_send;
  {
    rtc::CritScope crit(&lock_);
    while (!packets_.empty()) {
      to_send.push_back(std::move(packets_.front()));
      packets_.pop_front();
    }
  }
  for (auto& packet : to_send) {
    webrtc::NetworkNode* receiver = nullptr;
    {
      rtc::CritScope crit(&lock_);
      receiver = routing_[packet->dest_endpoint_id];
    }
    RTC_CHECK(receiver);
    // We don't want to keep the lock here. Otherwise we would get a deadlock if
    // the receiver tries to push a new packet.
    receiver->SendPacket(std::move(packet));
  }
}

void TransparentNetworkNode::SetReceiver(int endpoint_id,
                                         webrtc::NetworkNode* node) {
  rtc::CritScope crit(&lock_);
  routing_[endpoint_id] = node;
}

EmulatedNetworkNode::EmulatedNetworkNode(
    int id,
    std::unique_ptr<NetworkBehaviorInterface> network_behavior)
    : webrtc::NetworkNode(id), network_behavior_(std::move(network_behavior)) {}

EmulatedNetworkNode::~EmulatedNetworkNode() = default;
EmulatedNetworkNode::StoredPacket::StoredPacket(
    uint64_t id,
    std::unique_ptr<webrtc::EmulatedIpPacket> packet,
    bool removed)
    : id(id), packet(std::move(packet)), removed(removed) {}
EmulatedNetworkNode::StoredPacket::~StoredPacket() = default;

void EmulatedNetworkNode::SendPacketInternal(
    std::unique_ptr<EmulatedIpPacket> packet) {
  rtc::CritScope crit(&crit_sect_);
  if (routing_.find(packet->dest_endpoint_id) == routing_.end())
    return;
  uint64_t packet_id = next_packet_id_++;
  bool sent = network_behavior_->EnqueuePacket(
      PacketInFlightInfo(packet->data.size() + packet_overhead_,
                         packet->sent_time.us(), packet_id));
  if (sent) {
    packets_.emplace_back(
        absl::make_unique<StoredPacket>(packet_id, std::move(packet), false));
  }
}

void EmulatedNetworkNode::Process(Timestamp cur_time) {
  std::vector<PacketDeliveryInfo> delivery_infos;
  {
    rtc::CritScope crit(&crit_sect_);
    absl::optional<int64_t> delivery_us =
        network_behavior_->NextDeliveryTimeUs();
    if (delivery_us && *delivery_us > cur_time.us())
      return;

    delivery_infos =
        network_behavior_->DequeueDeliverablePackets(cur_time.us());
  }
  for (PacketDeliveryInfo& delivery_info : delivery_infos) {
    StoredPacket* packet = nullptr;
    webrtc::NetworkNode* receiver = nullptr;
    {
      rtc::CritScope crit(&crit_sect_);
      for (std::unique_ptr<StoredPacket>& stored_packet : packets_) {
        if (stored_packet->id == delivery_info.packet_id) {
          packet = stored_packet.get();
          break;
        }
      }
      RTC_CHECK(packet);
      RTC_DCHECK(!packet->removed);
      receiver = routing_[packet->packet->dest_endpoint_id];
      packet->removed = true;
    }
    RTC_CHECK(receiver);
    // We don't want to keep the lock here. Otherwise we would get a deadlock if
    // the receiver tries to push a new packet.
    packet->packet->sent_time = Timestamp::us(delivery_info.receive_time_us);
    receiver->SendPacket(std::move(packet->packet));
    {
      rtc::CritScope crit(&crit_sect_);
      while (!packets_.empty() && packets_.front()->removed) {
        packets_.pop_front();
      }
    }
  }
}

void EmulatedNetworkNode::SetReceiver(int endpoint_id,
                                      webrtc::NetworkNode* node) {
  rtc::CritScope crit(&crit_sect_);
  routing_[endpoint_id] = node;
}

EndpointNodeImpl::EndpointNodeImpl(int endpoint_id,
                                   webrtc::NetworkNode* entry_node,
                                   webrtc::NetworkNode* exit_node)
    : webrtc::EndpointNode(endpoint_id),
      connected_endpoint_(nullptr),
      entry_node_(entry_node),
      exit_node_(exit_node),
      router_(absl::make_unique<Router>()),
      network_thread_(nullptr) {
  proxy_node_ = absl::make_unique<ProxyNode>(this);
  exit_node_->SetReceiver(endpoint_id, proxy_node_.get());
}

EndpointNodeImpl::~EndpointNodeImpl() = default;

void EndpointNodeImpl::SendPacket(const rtc::SocketAddress& from,
                                  const rtc::SocketAddress& to,
                                  rtc::CopyOnWriteBuffer packet,
                                  Timestamp sent_time) {
  if (peer_local_addr_) {
    RTC_CHECK(from.ip() == peer_local_addr_.value().ip());
  }
  RTC_CHECK(connected_endpoint_);
  entry_node_->SendPacket(absl::make_unique<EmulatedIpPacket>(
      from, to, connected_endpoint_->GetId(), std::move(packet), sent_time,
      std::vector<int>{}));
}

bool EndpointNodeImpl::BindSocket(rtc::SocketAddress* local_addr,
                                  webrtc::FakeNetworkSocket* socket) {
  bool result = router_->RegisterSocket(local_addr, socket);
  if (result) {
    RTC_LOG(INFO) << "Socket " << socket->GetId() << " is binded to endpoint "
                  << GetId() << " on port " << local_addr->port();
  }
  return result;
}

void EndpointNodeImpl::UnbindSocket(uint16_t port) {
  router_->UnregisterSocket(port);
}

absl::optional<rtc::SocketAddress> EndpointNodeImpl::GetPeerLocalAddress()
    const {
  return peer_local_addr_;
}

webrtc::NetworkNode* EndpointNodeImpl::GetEntryNode() {
  return entry_node_;
}
webrtc::NetworkNode* EndpointNodeImpl::GetExitNode() {
  return exit_node_;
}
void EndpointNodeImpl::SetConnectedEndpoint(webrtc::EndpointNode* endpoint) {
  RTC_CHECK(!connected_endpoint_ || connected_endpoint_ == endpoint);
  connected_endpoint_ = endpoint;
}
void EndpointNodeImpl::SetNetworkThread(rtc::Thread* network_thread) {
  RTC_CHECK(!network_thread_);
  network_thread_ = network_thread;
}

EndpointNodeImpl::Router::Router() : next_port_(kFirstEphemeralPort) {}
EndpointNodeImpl::Router::~Router() = default;

bool EndpointNodeImpl::Router::RegisterSocket(
    rtc::SocketAddress* local_addr,
    webrtc::FakeNetworkSocket* socket) {
  rtc::CritScope crit(&lock_);
  if (local_addr->port() == 0) {
    for (int i = 0;
         i < std::numeric_limits<uint16_t>::max() - kFirstEphemeralPort; ++i) {
      local_addr->SetPort(next_port_);
      next_port_++;
      if (next_port_ >= std::numeric_limits<uint16_t>::max()) {
        next_port_ = kFirstEphemeralPort;
      }
      if (port_to_socket_.find(next_port_) == port_to_socket_.end()) {
        break;
      }
    }
  }
  return port_to_socket_.insert({local_addr->port(), socket}).second;
}

void EndpointNodeImpl::Router::UnregisterSocket(uint16_t port) {
  rtc::CritScope crit(&lock_);
  port_to_socket_.erase(port);
}

webrtc::FakeNetworkSocket* EndpointNodeImpl::Router::GetSocket(uint16_t port) {
  rtc::CritScope crit(&lock_);
  auto socket = port_to_socket_.find(port);
  if (socket == port_to_socket_.end()) {
    RTC_LOG(LS_ERROR) << "No socket listening on port " << port;
    return nullptr;
  }
  return socket->second;
}

EndpointNodeImpl::ProxyNode::ProxyNode(EndpointNodeImpl* endpoint)
    : webrtc::NetworkNode(endpoint->GetId()), endpoint_(endpoint) {}
EndpointNodeImpl::ProxyNode::~ProxyNode() = default;

void EndpointNodeImpl::ProxyNode::SendPacketInternal(
    std::unique_ptr<EmulatedIpPacket> packet) {
  RTC_CHECK(packet->dest_endpoint_id == endpoint_->GetId())
      << "Routing error: wrong destination endpoint. Destination id: "
      << packet->dest_endpoint_id << "; Receiver id: " << endpoint_->GetId();
  webrtc::FakeNetworkSocket* socket =
      endpoint_->router_->GetSocket(packet->to.port());
  if (!socket) {
    RTC_LOG(WARNING) << "PACKET DROPPED: No socket registered in "
                     << endpoint_->GetId() << " on port " << packet->to.port();
    return;
  }
  RTC_CHECK(endpoint_->network_thread_);
  endpoint_->network_thread_->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&webrtc::FakeNetworkSocket::DeliverPacket,
                               socket, packet->data, packet->from));
}

}  // namespace test
}  // namespace webrtc
