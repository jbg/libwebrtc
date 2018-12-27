/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/network_emulation_manager.h"

#include <algorithm>
#include <memory>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/pc/e2e/network.h"

namespace webrtc {
namespace test {

RepeatedActivity::RepeatedActivity(TimeDelta interval,
                                   std::function<void(Timestamp)> function)
    : interval_(interval), function_(std::move(function)) {}
RepeatedActivity::~RepeatedActivity() = default;

void RepeatedActivity::Poll(Timestamp cur_time) {
  if (last_call_time_ == Timestamp::MinusInfinity()) {
    // First call.
    function_(cur_time);
    last_call_time_ = cur_time;
  }
  if (cur_time >= NextTime(last_call_time_)) {
    // Call only if next time arrived.
    function_(cur_time);
  }
}

Timestamp RepeatedActivity::NextTime(Timestamp cur_time) {
  return cur_time + interval_;
}

NetworkEmulationManagerImpl::NetworkEmulationManagerImpl(webrtc::Clock* clock)
    : clock_(clock),
      next_node_id_(1),
      state_(State::kIdle),
      last_log_time_(Timestamp::MinusInfinity()),
      log_interval_(TimeDelta::ms(500)),
      task_queue_("network_emulation_manager") {}
NetworkEmulationManagerImpl::~NetworkEmulationManagerImpl() = default;

webrtc::NetworkNode* NetworkEmulationManagerImpl::CreateTransparentNode() {
  CheckIdle();

  return RegisterNode(
      absl::make_unique<TransparentNetworkNode>(next_node_id_++));
}

webrtc::NetworkNode* NetworkEmulationManagerImpl::CreateEmulatedNode(
    std::unique_ptr<webrtc::NetworkBehaviorInterface> network_behavior) {
  CheckIdle();

  return RegisterNode(absl::make_unique<EmulatedNetworkNode>(
      next_node_id_++, std::move(network_behavior)));
}

webrtc::NetworkNode* NetworkEmulationManagerImpl::RegisterNode(
    std::unique_ptr<webrtc::NetworkNode> node) {
  CheckIdle();

  NetworkNode* out = node.get();
  network_nodes_.push_back(std::move(node));
  repeated_activities_.push_back(absl::make_unique<RepeatedActivity>(
      TimeDelta::ms(1), [out](Timestamp cur_time) { out->Process(cur_time); }));
  return out;
}

webrtc::EndpointNode* NetworkEmulationManagerImpl::CreateEndpoint(
    webrtc::NetworkNode* entry_node,
    webrtc::NetworkNode* exit_node) {
  CheckIdle();

  auto node = absl::make_unique<webrtc::test::EndpointNodeImpl>(
      next_node_id_++, entry_node, exit_node);
  webrtc::EndpointNode* out = node.get();
  endpoints_.push_back(std::move(node));
  return out;
}

void NetworkEmulationManagerImpl::CreateRoute(webrtc::EndpointNode* from,
                                              webrtc::EndpointNode* to) {
  CheckIdle();

  RTC_LOG(INFO) << "Creating route from " << from->GetId() << " to "
                << to->GetId();
  SetConnectedEndpoint(from, to);
  SetConnectedEndpoint(to, from);
  if (GetEntryNode(from) == GetExitNode(to)) {
    // If from and to are based on same network node as entrance and exit we
    // need do nothing. They are already connected.
    return;
  }
  GetEntryNode(from)->SetReceiver(to->GetId(), GetExitNode(to));
}

void NetworkEmulationManagerImpl::CreateRoute(
    webrtc::EndpointNode* from,
    std::vector<webrtc::NetworkNode*> via_nodes,
    webrtc::EndpointNode* to) {
  CheckIdle();

  RTC_CHECK(!via_nodes.empty());
  SetConnectedEndpoint(from, to);
  SetConnectedEndpoint(to, from);
  GetEntryNode(from)->SetReceiver(to->GetId(), via_nodes[0]);
  for (size_t i = 0; i + 1 < via_nodes.size(); ++i)
    via_nodes[i]->SetReceiver(to->GetId(), via_nodes[i + 1]);
  via_nodes[via_nodes.size() - 1]->SetReceiver(to->GetId(), GetExitNode(to));
}

rtc::Thread* NetworkEmulationManagerImpl::CreateNetworkThread(
    std::vector<webrtc::EndpointNode*> endpoints) {
  CheckIdle();

  FakeNetworkSocketServer* socket_server = CreateSocketServer(endpoints);
  std::unique_ptr<rtc::Thread> network_thread =
      absl::make_unique<rtc::Thread>(socket_server);
  network_thread->SetName("network_thread" + std::to_string(threads_.size()),
                          nullptr);
  network_thread->Start();
  rtc::Thread* out = network_thread.get();
  threads_.push_back(std::move(network_thread));
  for (auto* endpoint : endpoints) {
    SetNetworkThread(endpoint, out);
  }
  return out;
}

void NetworkEmulationManagerImpl::Start() {
  {
    rtc::CritScope crit(&lock_);
    state_ = kRunning;
    last_log_time_ = Now();
  }
  MakeHeartBeat();
}

void NetworkEmulationManagerImpl::Stop() {
  rtc::CritScope crit(&lock_);
  state_ = State::kStopping;
  last_log_time_ = Timestamp::MinusInfinity();
}

FakeNetworkSocketServer* NetworkEmulationManagerImpl::CreateSocketServer(
    std::vector<webrtc::EndpointNode*> endpoints) {
  CheckIdle();

  auto socket_server = absl::make_unique<FakeNetworkSocketServer>(
      next_node_id_++ * 1000, clock_, endpoints);
  FakeNetworkSocketServer* out = socket_server.get();
  socket_servers_.push_back(std::move(socket_server));
  return out;
}

void NetworkEmulationManagerImpl::MakeHeartBeat() {
  {
    rtc::CritScope crit(&lock_);
    if (state_ != kRunning) {
      state_ = kIdle;
      return;
    }
    if (last_log_time_ + log_interval_ < Now()) {
      last_log_time_ = Now();
      RTC_LOG(INFO) << "Network emulation manager heartbeat";
    }
  }
  Timestamp current_time = Now();
  Timestamp next_time = current_time + TimeDelta::PlusInfinity();
  for (auto& activity : repeated_activities_) {
    activity->Poll(current_time);
    next_time = std::min(next_time, activity->NextTime(current_time));
  }
  TimeDelta wait_time = next_time - current_time;
  RTC_CHECK(wait_time.ns() > 0);
  task_queue_.PostDelayedTask([this]() { MakeHeartBeat(); },
                              wait_time.ms<int64_t>());
}

void NetworkEmulationManagerImpl::CheckIdle() const {
  rtc::CritScope crit(&lock_);
  RTC_CHECK(state_ == State::kIdle);
}

Timestamp NetworkEmulationManagerImpl::Now() const {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

}  // namespace test
}  // namespace webrtc
