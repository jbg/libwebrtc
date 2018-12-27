/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/fake_network_socket_server.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "api/test/network.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/network.h"

namespace webrtc {
namespace test {

std::string ToString(const rtc::SocketAddress& addr) {
  return addr.HostAsURIString() + ":" + std::to_string(addr.port());
}

FakeNetworkSocket::FakeNetworkSocket(int id,
                                     webrtc::Clock* clock,
                                     FakeNetworkSocketServer* socket_server)
    : webrtc::FakeNetworkSocket(id),
      clock_(clock),
      socket_server_(socket_server),
      state_(CS_CLOSED),
      error_(0) {}
FakeNetworkSocket::~FakeNetworkSocket() {
  Close();
}
FakeNetworkSocket::Packet::Packet() = default;
FakeNetworkSocket::Packet::Packet(Packet&& packet) = default;
FakeNetworkSocket::Packet::~Packet() = default;

void FakeNetworkSocket::DeliverPacket(rtc::CopyOnWriteBuffer packet,
                                      const rtc::SocketAddress& source_addr) {
  Packet p;
  p.source_addr = source_addr;
  p.data = std::vector<uint8_t>(packet.data(), packet.data() + packet.size());
  packet_queue_.push_back(std::move(p));
  SignalReadEvent(this);
}

rtc::SocketAddress FakeNetworkSocket::GetLocalAddress() const {
  return local_addr_;
}

rtc::SocketAddress FakeNetworkSocket::GetRemoteAddress() const {
  return remote_addr_;
}

int FakeNetworkSocket::Bind(const rtc::SocketAddress& addr) {
  if (!local_addr_.IsNil()) {
    RTC_LOG(LS_ERROR) << "Socket already bound to address: "
                      << ToString(local_addr_);
    error_ = EINVAL;
    return -1;
  }
  local_addr_ = addr;
  endpoint_ = socket_server_->GetEndpointNode(addr);
  if (!endpoint_) {
    local_addr_.Clear();
    RTC_LOG(LS_ERROR) << "No endpoint for address: " << ToString(addr);
    error_ = EADDRINUSE;
    return 1;
  }
  if (!endpoint_->BindSocket(&local_addr_, this)) {
    local_addr_.Clear();
    RTC_LOG(LS_ERROR) << "Cannot bind to in-use address: " << ToString(addr);
    error_ = EADDRINUSE;
    return 1;
  }
  return 0;
}

int FakeNetworkSocket::BindDefault(const rtc::SocketAddress& addr) {
  std::string ip = addr.ipaddr().family() == AF_INET ? "0.0.0.0" : "::";
  return Bind(rtc::SocketAddress(ip, 0));
}

int FakeNetworkSocket::Connect(const rtc::SocketAddress& addr) {
  if (!remote_addr_.IsNil()) {
    RTC_LOG(LS_ERROR) << "Socket already connected to address: "
                      << ToString(remote_addr_);
    error_ = EISCONN;
    return -1;
  }
  if (local_addr_.IsNil()) {
    int result = BindDefault(addr);
    if (result != 0) {
      return result;
    }
  }
  remote_addr_ = addr;
  state_ = CS_CONNECTED;
  return 0;
}

int FakeNetworkSocket::Send(const void* pv, size_t cb) {
  if (state_ != CS_CONNECTED) {
    RTC_LOG(LS_ERROR) << "Socket cannot send: not connected";
    error_ = ENOTCONN;
    return -1;
  }
  return SendTo(pv, cb, remote_addr_);
}

int FakeNetworkSocket::SendTo(const void* pv,
                              size_t cb,
                              const rtc::SocketAddress& addr) {
  if (local_addr_.IsNil()) {
    int result = BindDefault(addr);
    if (result != 0) {
      local_addr_.Clear();
      error_ = EADDRINUSE;
      return result;
    }
  }
  rtc::CopyOnWriteBuffer packet(static_cast<const uint8_t*>(pv), cb);
  endpoint_->SendPacket(local_addr_, addr, packet,
                        Timestamp::us(clock_->TimeInMicroseconds()));
  return cb;
}

int FakeNetworkSocket::Recv(void* pv, size_t cb, int64_t* timestamp) {
  rtc::SocketAddress paddr;
  return RecvFrom(pv, cb, &paddr, timestamp);
}

int FakeNetworkSocket::RecvFrom(void* pv,
                                size_t cb,
                                rtc::SocketAddress* paddr,
                                int64_t* timestamp) {
  if (timestamp) {
    *timestamp = -1;
  }
  if (packet_queue_.empty()) {
    error_ = EAGAIN;
    return -1;
  }

  Packet packet = std::move(packet_queue_.front());
  packet_queue_.pop_front();
  *paddr = packet.source_addr;
  size_t data_read = std::min(cb, packet.data.size());
  memcpy(pv, packet.data.data(), data_read);

  if (data_read < packet.data.size()) {
    packet.data.erase(packet.data.begin(), packet.data.begin() + data_read);
    packet_queue_.push_front(std::move(packet));
    SignalReadEvent(this);
  }

  return static_cast<int>(data_read);
}

int FakeNetworkSocket::Listen(int backlog) {
  RTC_CHECK(false) << "Listen() isn't valid for SOCK_DGRAM";
}

rtc::AsyncSocket* FakeNetworkSocket::Accept(rtc::SocketAddress* /*paddr*/) {
  RTC_CHECK(false) << "Accept() isn't valid for SOCK_DGRAM";
}

int FakeNetworkSocket::Close() {
  state_ = CS_CLOSED;
  if (!local_addr_.IsNil()) {
    endpoint_->UnbindSocket(local_addr_.port());
  }
  local_addr_.Clear();
  remote_addr_.Clear();
  return 0;
}

int FakeNetworkSocket::GetError() const {
  RTC_CHECK(error_ == 0);
  return error_;
}

void FakeNetworkSocket::SetError(int error) {
  RTC_CHECK(error == 0);
  error_ = error;
}

rtc::AsyncSocket::ConnState FakeNetworkSocket::GetState() const {
  return state_;
}

int FakeNetworkSocket::GetOption(Option opt, int* value) {
  auto it = options_map_.find(opt);
  if (it == options_map_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int FakeNetworkSocket::SetOption(Option opt, int value) {
  options_map_[opt] = value;
  return 0;
}

FakeNetworkSocketServer::FakeNetworkSocketServer(
    int socket_id_init_value,
    webrtc::Clock* clock,
    std::vector<webrtc::EndpointNode*> endpoints)
    : next_socket_id_(socket_id_init_value),
      clock_(clock),
      endpoints_(std::move(endpoints)),
      wakeup_(/*manual_reset=*/false, /*initially_signaled=*/false) {}
FakeNetworkSocketServer::~FakeNetworkSocketServer() = default;

void FakeNetworkSocketServer::OnMessageQueueDestroyed() {
  msg_queue_ = nullptr;
}

EndpointNode* FakeNetworkSocketServer::GetEndpointNode(
    const rtc::SocketAddress& addr) {
  webrtc::EndpointNode* default_endpoint = nullptr;
  for (auto* endpoint : endpoints_) {
    absl::optional<rtc::SocketAddress> peerLocalAddress =
        endpoint->GetPeerLocalAddress();
    if (peerLocalAddress) {
      if (peerLocalAddress == addr) {
        return endpoint;
      }
    } else if (!default_endpoint) {
      default_endpoint = endpoint;
    }
  }
  RTC_CHECK(default_endpoint)
      << "No network found for address and no default provided"
      << ToString(addr);
  return default_endpoint;
}

rtc::Socket* FakeNetworkSocketServer::CreateSocket(int /*family*/,
                                                   int /*type*/) {
  RTC_CHECK(false) << "Only async sockets are supported";
}

rtc::AsyncSocket* FakeNetworkSocketServer::CreateAsyncSocket(int family,
                                                             int type) {
  RTC_DCHECK(family == AF_INET || family == AF_INET6);
  // We support only UDP sockets for now.
  RTC_DCHECK(type == SOCK_DGRAM);
  return new FakeNetworkSocket(next_socket_id_++, clock_, this);
}

void FakeNetworkSocketServer::SetMessageQueue(rtc::MessageQueue* msg_queue) {
  msg_queue_ = msg_queue;
  if (msg_queue_) {
    msg_queue_->SignalQueueDestroyed.connect(
        this, &FakeNetworkSocketServer::OnMessageQueueDestroyed);
  }
}

bool FakeNetworkSocketServer::Wait(int cms, bool /*process_io*/) {
  RTC_DCHECK(msg_queue_ == rtc::Thread::Current());
  // Note: we don't need to do anything with |process_io| since we don't have
  // any real I/O. Received packets come in the form of queued messages, so
  // MessageQueue will ensure WakeUp is called if another thread sends a
  // packet.
  wakeup_.Wait(cms);
  return true;
}

void FakeNetworkSocketServer::WakeUp() {
  wakeup_.Set();
}

}  // namespace test
}  // namespace webrtc
