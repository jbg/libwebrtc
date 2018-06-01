/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/fakemetricsobserver.h"
#include "p2p/base/fakeportallocator.h"
#include "p2p/base/mockicetransport.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/port.h"
#include "p2p/base/regatheringcontroller.h"
#include "p2p/base/stunserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"

namespace {

const int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
                            cricket::PORTALLOCATOR_DISABLE_RELAY |
                            cricket::PORTALLOCATOR_DISABLE_TCP;
// The address of the public STUN server.
const rtc::SocketAddress kStunAddr("99.99.99.1", cricket::STUN_SERVER_PORT);
// The addresses for the public TURN server.
const rtc::SocketAddress kTurnUdpIntAddr("99.99.99.3",
                                         cricket::STUN_SERVER_PORT);
const cricket::RelayCredentials kRelayCredentials("test", "test");
const char kIceUfrag[] = "UF00";
const char kIcePwd[] = "TESTICEPWD00000000000000";

}  // namespace

namespace webrtc {

class RegatheringControllerTest : public testing::Test,
                                  public sigslot::has_slots<> {
 public:
  RegatheringControllerTest()
      : ice_transport_(new cricket::MockIceTransport()),
        allocator_(
            new cricket::FakePortAllocator(rtc::Thread::Current(), nullptr)) {
    BasicRegatheringController::Config regathering_config(0, rtc::nullopt);
    regathering_controller_.reset(new BasicRegatheringController(
        ice_transport_.get(), regathering_config, rtc::Thread::Current()));
  }

  // Initializes the allocator and gathers candidates once by StartGettingPorts.
  // The regathering controller is initialized with the allocator session
  // after the session is cleared. Only after this, we would be able to
  // regather. See the comments for BasicRegatheringController in
  // regatheringcontroller.h.
  void InitializeAndGatherOnce() {
    cricket::ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    cricket::RelayServerConfig turn_server(cricket::RELAY_TURN);
    turn_server.credentials = kRelayCredentials;
    turn_server.ports.push_back(
        cricket::ProtocolAddress(kTurnUdpIntAddr, cricket::PROTO_UDP));
    std::vector<cricket::RelayServerConfig> turn_servers(1, turn_server);
    allocator_->set_flags(kOnlyLocalPorts);
    allocator_->SetConfiguration(stun_servers, turn_servers, 0 /* pool size */,
                                 false /* prune turn ports */);
    allocator_session_ = allocator_->CreateSession(
        "test", cricket::ICE_CANDIDATE_COMPONENT_RTP, kIceUfrag, kIcePwd);
    // The gathering will take place on the current thread and the following
    // call of StartGettingPorts is blocking. We will not ClearGettingPorts
    // prematurely.
    allocator_session_->StartGettingPorts();
    allocator_session_->ClearGettingPorts();
    allocator_session_->SignalIceRegathering.connect(
        this, &RegatheringControllerTest::OnIceRegathering);
    regathering_controller_->set_allocator_session(allocator_session_.get());
  }

  void OnIceRegathering(cricket::PortAllocatorSession* allocator_session,
                        cricket::IceRegatheringReason reason) {
    ++count_[reason];
  }

  void SetConfig(const BasicRegatheringController::Config& config) {
    regathering_controller_->SetConfig(config);
  }

  void ScheduleRegatheringOnAllNetworks(bool repeated) {
    regathering_controller_->ScheduleRegatheringOnAllNetworks(repeated);
  }

  void ScheduleRegatheringOnFailedNetworks(bool repeated) {
    regathering_controller_->ScheduleRegatheringOnFailedNetworks(repeated);
  }

  void CancelScheduledRegatheringOnAllNetworks() {
    regathering_controller_->CancelScheduledRegatheringOnAllNetworks();
  }

  void CancelScheduledRegatheringOnFailedNetworks() {
    regathering_controller_->CancelScheduledRegatheringOnFailedNetworks();
  }

  int GetRegatheringReasonCount(cricket::IceRegatheringReason reason) {
    return count_[reason];
  }

 private:
  std::unique_ptr<cricket::IceTransportInternal> ice_transport_;
  std::unique_ptr<BasicRegatheringController> regathering_controller_;
  std::unique_ptr<cricket::PortAllocator> allocator_;
  std::unique_ptr<cricket::PortAllocatorSession> allocator_session_;
  std::map<cricket::IceRegatheringReason, int> count_;
};

// Tests that ICE regathering can be canceled and resumed once via the
// regathering controller.
TEST_F(RegatheringControllerTest,
       IceRegatheringOnAllNetworksCanBeCanceledAndResumedOnce) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      kRegatherInterval, regather_all_networks_interval_range);
  SetConfig(config);
  const int kNetworkGatherDurationShort = 3000;
  ScheduleRegatheringOnAllNetworks(true /* repeated*/);
  SIMULATED_WAIT(false, kRegatherInterval - 1, clock);
  // Expect no regathering.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  SIMULATED_WAIT(false, kNetworkGatherDurationShort - kRegatherInterval + 1,
                 clock);
  // Expect regathering to happen once in 3s with 2s interval.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  CancelScheduledRegatheringOnAllNetworks();
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect no regathering happened in the last 3s.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  ScheduleRegatheringOnAllNetworks(false /* repeated */);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen only once in 11s.
  EXPECT_EQ(2, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that ICE regathering can be canceled and resumed to repeat via the
// regathering controller.
TEST_F(RegatheringControllerTest,
       IceRegatheringOnAllNetworksCanBeCanceledAndResumedToRepeat) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      kRegatherInterval, regather_all_networks_interval_range);
  SetConfig(config);

  const int kNetworkGatherDurationShort = 3000;
  ScheduleRegatheringOnAllNetworks(true /* repeated*/);
  SIMULATED_WAIT(false, kRegatherInterval - 1, clock);
  // Expect no regathering is done.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  SIMULATED_WAIT(false, kNetworkGatherDurationShort - kRegatherInterval + 1,
                 clock);
  // Expect regathering to happen once in 3s with 2s interval.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  CancelScheduledRegatheringOnAllNetworks();
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect no regathering happened in the last 3s.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  ScheduleRegatheringOnAllNetworks(true /* repeated */);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for another 5 times in 11s with 2s
  EXPECT_EQ(6, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that the schedule of ICE regathering can be canceled and replaced by a
// new repeating schedule.
TEST_F(RegatheringControllerTest,
       PreviousScheduleOfIceRegatheringCanBeReplacedByNewRepeatingSchedule) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      kRegatherIntervalShort, regather_all_networks_interval_range);
  SetConfig(config);

  const int kNetworkGatherDurationShort = 3000;
  CancelScheduledRegatheringOnAllNetworks();
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect no regathering happened in the last 3s after regathering is
  // canceled.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  // Resume the regathering.
  ScheduleRegatheringOnAllNetworks(true /* repeated */);
  SIMULATED_WAIT(false, kRegatherIntervalShort - 500, clock);
  // Reschedule repeating regathering. We should not have regathered yet.
  const int kRegatherIntervalLong = 5000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  SetConfig(config);
  ScheduleRegatheringOnAllNetworks(true /* repeated */);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen twice in the last 13.5s with 5s interval.
  EXPECT_EQ(2, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
}

TEST_F(RegatheringControllerTest,
       IceRegatheringOnFailedNetworksCanBeCanceledAndResumed) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      kRegatherInterval, regather_all_networks_interval_range);
  SetConfig(config);
  const int kNetworkGatherDurationShort = 3000;
  ScheduleRegatheringOnFailedNetworks(true /* repeated*/);
  SIMULATED_WAIT(false, kRegatherInterval - 1, clock);
  // Expect no regathering is done.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, kNetworkGatherDurationShort - kRegatherInterval + 1,
                 clock);
  // Expect regathering to happen once in 3s with 2s interval.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  CancelScheduledRegatheringOnFailedNetworks();
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect no regathering happened in the last 3s.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  ScheduleRegatheringOnFailedNetworks(true /* repeated */);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for another 5 times once in 11s.
  EXPECT_EQ(6, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  CancelScheduledRegatheringOnFailedNetworks();
  ScheduleRegatheringOnFailedNetworks(false /* repeated */);
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for only once in the last 11s.
  EXPECT_EQ(7, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that canceling the regathering on failed networks does not cancel the
// schedule on all networks.
TEST_F(RegatheringControllerTest,
       CancelingRegatheringOnFailedNetworksDoesNotCancelOnAllNetworks) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      kRegatherIntervalShort, regather_all_networks_interval_range);
  SetConfig(config);
  ScheduleRegatheringOnAllNetworks(true /* repeated*/);
  ScheduleRegatheringOnFailedNetworks(true /* repeated*/);
  SIMULATED_WAIT(false, 1000, clock);
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  CancelScheduledRegatheringOnFailedNetworks();
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for 5 times for all networks in the last 11s
  // with 2s interval.
  EXPECT_EQ(5, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that canceling the regathering on all networks does not cancel the
// schedule on failed networks.
TEST_F(RegatheringControllerTest,
       CancelingRegatheringOnAllNetworksDoesNotCancelOnFailedNetworks) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      kRegatherIntervalShort, regather_all_networks_interval_range);
  SetConfig(config);
  ScheduleRegatheringOnAllNetworks(true /* repeated*/);
  ScheduleRegatheringOnFailedNetworks(true /* repeated*/);
  SIMULATED_WAIT(false, 1000, clock);
  // Expect no regathering.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  CancelScheduledRegatheringOnAllNetworks();
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for 5 times for failed networks in the last
  // 11s with 2s interval.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(5, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that changing the config of ICE regathering does not cancel one-time
// schedule.
TEST_F(RegatheringControllerTest,
       ChangingConfigDoesNotCancelExistingSchedules) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      kRegatherIntervalShort, regather_all_networks_interval_range);
  SetConfig(config);
  ScheduleRegatheringOnAllNetworks(false /* repeated*/);
  ScheduleRegatheringOnFailedNetworks(false /* repeated*/);
  SIMULATED_WAIT(false, 1000, clock);
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 5000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  SetConfig(config);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen once in the last 11s for all networks and
  // failed networks respectively.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that changing the config of ICE regathering on all networks cancels and
// reschedules recurring regathering on all networks, but does not impact the
// schedule on failed networks.
TEST_F(RegatheringControllerTest,
       ChangingConfigCancelsAndReschedulesRecurringSchedulesOnAllNetworks) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      kRegatherIntervalShort, regather_all_networks_interval_range);
  SetConfig(config);
  ScheduleRegatheringOnAllNetworks(true /* repeated*/);
  ScheduleRegatheringOnFailedNetworks(true /* repeated*/);
  SIMULATED_WAIT(false, 1000, clock);
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 5000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen twice for all networks in the last 11s with 5s
  // interval.
  EXPECT_EQ(2, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  // Expect the schedule on failed networks is not impacted and regathering to
  // happen for 5 times in the last 11s with 2s interval.
  EXPECT_EQ(5, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that changing the config of ICE regathering on failed networks cancels
// and reschedules recurring regathering on failed networks, but does not impact
// the schedule on all networks.
TEST_F(RegatheringControllerTest,
       ChangingConfigCancelsAndReschedulesRecurringSchedulesOnFailedNetworks) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      kRegatherIntervalShort, regather_all_networks_interval_range);
  SetConfig(config);
  ScheduleRegatheringOnAllNetworks(true /* repeated*/);
  ScheduleRegatheringOnFailedNetworks(true /* repeated*/);
  SIMULATED_WAIT(false, 1000, clock);
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 5000;
  config.regather_on_failed_networks_interval = kRegatherIntervalLong;
  SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen twice for failed networks in the last 11s with
  // 5s interval.
  EXPECT_EQ(2, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  // Expect the schedule on all networks is not impacted and regathering to
  // happen for 5 times in the last 11s with 2s interval.
  EXPECT_EQ(5, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
}

}  // namespace webrtc
