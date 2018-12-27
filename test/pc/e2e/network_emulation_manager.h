/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_NETWORK_EMULATION_MANAGER_H_
#define TEST_PC_E2E_NETWORK_EMULATION_MANAGER_H_

#include <memory>
#include <utility>
#include <vector>

#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/fake_network_socket_server.h"

namespace webrtc {
namespace test {

class RepeatedActivity {
 public:
  RepeatedActivity(TimeDelta interval, std::function<void(Timestamp)> function);
  ~RepeatedActivity();

  void Poll(Timestamp cur_time);
  Timestamp NextTime(Timestamp cur_time);

 private:
  TimeDelta interval_;
  std::function<void(Timestamp)> function_;
  Timestamp last_call_time_ = Timestamp::MinusInfinity();
};

class NetworkEmulationManagerImpl : public webrtc::NetworkEmulationManager {
 public:
  explicit NetworkEmulationManagerImpl(webrtc::Clock* clock);
  ~NetworkEmulationManagerImpl() override;

  webrtc::NetworkNode* CreateTransparentNode() override;
  webrtc::NetworkNode* CreateEmulatedNode(
      std::unique_ptr<webrtc::NetworkBehaviorInterface> network_behavior)
      override;
  webrtc::NetworkNode* RegisterNode(
      std::unique_ptr<webrtc::NetworkNode> node) override;

  webrtc::EndpointNode* CreateEndpoint(webrtc::NetworkNode* entry_node,
                                       webrtc::NetworkNode* exit_node) override;

  void CreateRoute(webrtc::EndpointNode* from,
                   webrtc::EndpointNode* to) override;
  void CreateRoute(webrtc::EndpointNode* from,
                   std::vector<webrtc::NetworkNode*> via_nodes,
                   webrtc::EndpointNode* to) override;

  rtc::Thread* CreateNetworkThread(
      std::vector<webrtc::EndpointNode*> endpoints) override;

  void Start() override;
  void Stop() override;

 private:
  enum State { kIdle, kStopping, kRunning };

  FakeNetworkSocketServer* CreateSocketServer(
      std::vector<webrtc::EndpointNode*> endpoints);
  void MakeHeartBeat();
  void CheckIdle() const;
  Timestamp Now() const;

  webrtc::Clock* const clock_;
  int next_node_id_;
  rtc::CriticalSection lock_;
  State state_ RTC_GUARDED_BY(lock_);

  // All objects can be added to the manager only when it is idle.
  std::vector<std::unique_ptr<webrtc::EndpointNode>> endpoints_;
  std::vector<std::unique_ptr<webrtc::NetworkNode>> network_nodes_;
  std::vector<std::unique_ptr<FakeNetworkSocketServer>> socket_servers_;
  std::vector<std::unique_ptr<rtc::Thread>> threads_;
  std::vector<std::unique_ptr<RepeatedActivity>> repeated_activities_;

  Timestamp last_log_time_ RTC_GUARDED_BY(lock_);
  TimeDelta log_interval_;

  // Must be the last field, so it will be deconstructed first as tasks
  // in the TaskQueue access other fields of the instance of this class.
  rtc::TaskQueue task_queue_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_NETWORK_EMULATION_MANAGER_H_
