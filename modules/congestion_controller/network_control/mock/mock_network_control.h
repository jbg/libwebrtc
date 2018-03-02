/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_MOCK_MOCK_NETWORK_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_MOCK_MOCK_NETWORK_CONTROL_H_

#include "modules/congestion_controller/network_control/include/network_control.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {
class MockNetworkControllerObserver : public NetworkControllerObserver {
 public:
  MOCK_METHOD1(OnCongestionWindow, void(CongestionWindow));
  MOCK_METHOD1(OnPacerConfig, void(PacerConfig));
  MOCK_METHOD1(OnProbeClusterConfig, void(ProbeClusterConfig));
  MOCK_METHOD1(OnTargetTransferRate, void(TargetTransferRate));
};

class MockNetworkController : public NetworkControllerInterface {
 public:
  MOCK_METHOD1(OnNetworkAvailability, void(NetworkAvailability));
  MOCK_METHOD1(OnNetworkRouteChange, void(NetworkRouteChange));
  MOCK_METHOD1(OnProcessInterval, void(ProcessInterval));
  MOCK_METHOD1(OnRemoteBitrateReport, void(RemoteBitrateReport));
  MOCK_METHOD1(OnRoundTripTimeUpdate, void(RoundTripTimeUpdate));
  MOCK_METHOD1(OnSentPacket, void(SentPacket));
  MOCK_METHOD1(OnStreamsConfig, void(StreamsConfig));
  MOCK_METHOD1(OnTargetRateConstraints, void(TargetRateConstraints));
  MOCK_METHOD1(OnTransportLossReport, void(TransportLossReport));
  MOCK_METHOD1(OnTransportPacketsFeedback, void(TransportPacketsFeedback));
};

class MockNetworkControllerFactory : public NetworkControllerFactoryInterface {
 public:
  MOCK_METHOD2(Create,
               NetworkControllerInterface::uptr(NetworkControllerObserver*,
                                                NetworkControllerConfig));
  MOCK_CONST_METHOD0(GetProcessInterval, TimeDelta());
};
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_MOCK_MOCK_NETWORK_CONTROL_H_
