/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_CONTROL_TESTER_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_CONTROL_TESTER_H_

#include <deque>
#include <functional>
#include <memory>

#include "api/optional.h"
#include "modules/congestion_controller/network_control/include/network_control.h"

namespace webrtc {
namespace test {
struct NetworkControlState {
  NetworkControlState();
  NetworkControlState(const NetworkControlState&);
  ~NetworkControlState();
  rtc::Optional<CongestionWindow> congestion_window;
  rtc::Optional<PacerConfig> pacer_config;
  rtc::Optional<ProbeClusterConfig> probe_config;
  rtc::Optional<TargetTransferRate> target_rate;
};

// Produces one packet per time delta
class SimpleTargetRateProducer {
 public:
  static SentPacket ProduceNext(const NetworkControlState& state,
                                Timestamp current_time,
                                TimeDelta time_delta);
};
class NetworkControlCacher : public NetworkControllerObserver {
 public:
  NetworkControlCacher();
  ~NetworkControlCacher() override;
  void OnCongestionWindow(CongestionWindow msg) override;
  void OnPacerConfig(PacerConfig msg) override;
  void OnProbeClusterConfig(ProbeClusterConfig) override;
  void OnTargetTransferRate(TargetTransferRate msg) override;
  NetworkControlState GetState() { return current_state_; }

 private:
  NetworkControlState current_state_;
};

class FeedbackBasedNetworkControllerTester {
 public:
  using PacketProducer = std::function<
      SentPacket(const NetworkControlState&, Timestamp, TimeDelta)>;
  FeedbackBasedNetworkControllerTester(
      FeedbackBasedNetworkControllerFactoryInterface* factory,
      NetworkControllerConfig initial_config);
  ~FeedbackBasedNetworkControllerTester();

  void RunSimulation(TimeDelta duration,
                     TimeDelta packet_interval,
                     DataRate actual_bandwidth,
                     TimeDelta propagation_delay,
                     PacketProducer next_packet);
  NetworkControlState GetState() { return cacher_.GetState(); }

 private:
  PacketResult SimulateSend(SentPacket packet,
                            TimeDelta time_delta,
                            TimeDelta propagation_delay,
                            DataRate actual_bandwidth);
  NetworkControlCacher cacher_;
  std::unique_ptr<FeedbackBasedNetworkControllerInterface> controller_;
  TimeDelta process_interval_;
  Timestamp current_time_ = Timestamp::s(100000);
  TimeDelta accumulated_delay_ = TimeDelta::ms(0);
  std::deque<PacketResult> received_packets_;
};
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_NETWORK_CONTROL_TESTER_H_
