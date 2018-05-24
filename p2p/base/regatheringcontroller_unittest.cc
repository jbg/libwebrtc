/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <vector>

#include "api/fakemetricsobserver.h"
#include "p2p/base/fakeportallocator.h"
#include "p2p/base/mockicetransport.h"
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
        metric_observer_(new rtc::RefCountedObject<FakeMetricsObserver>()) {
    allocator_.reset(
        new cricket::FakePortAllocator(rtc::Thread::Current(), nullptr));
    BasicRegatheringController::Config regathering_config(0, rtc::nullopt);
    regathering_controller_.reset(new BasicRegatheringController(
        ice_transport_.get(), regathering_config, rtc::Thread::Current()));
  }

  void InitializeAndGatherOnce() {
    cricket::ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    cricket::RelayServerConfig turn_server(cricket::RELAY_TURN);
    turn_server.credentials = kRelayCredentials;
    turn_server.ports.push_back(
        cricket::ProtocolAddress(kTurnUdpIntAddr, cricket::PROTO_UDP));
    std::vector<cricket::RelayServerConfig> turn_servers(1, turn_server);
    int pool_size = 1;
    allocator_->set_flags(kOnlyLocalPorts);
    allocator_->SetMetricsObserver(metric_observer());
    allocator_->SetConfiguration(stun_servers, turn_servers, pool_size, false);
    allocator_session_ =
        allocator_->TakePooledSession("test", 0, kIceUfrag, kIcePwd);
    allocator_session_->ClearGettingPorts();
    regathering_controller_->set_allocator_session(allocator_session_.get());
    allocator_session_->SignalIceRegathering.connect(
        this, &RegatheringControllerTest::OnIceRegathering);
  }

  void OnIceRegathering(cricket::PortAllocatorSession* allocator_session,
                        cricket::IceRegatheringReason reason) {
    metric_observer_->IncrementEnumCounter(
        webrtc::kEnumCounterIceRegathering, static_cast<int>(reason),
        static_cast<int>(cricket::IceRegatheringReason::MAX_VALUE));
  }
  FakeMetricsObserver* metric_observer() const {
    return metric_observer_.get();
  }
  BasicRegatheringController* regathering_controller() {
    return regathering_controller_.get();
  }

 private:
  std::unique_ptr<cricket::IceTransportInternal> ice_transport_;
  std::unique_ptr<cricket::PortAllocator> allocator_;
  std::unique_ptr<cricket::PortAllocatorSession> allocator_session_;
  rtc::scoped_refptr<FakeMetricsObserver> metric_observer_;
  std::unique_ptr<BasicRegatheringController> regathering_controller_;
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
  regathering_controller()->set_config(config);
  const int kNetworkGatherDurationShort = 3000;
  regathering_controller()->ScheduleRegatheringOnAllNetworks(
      true /* repeated*/);
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect regathering to happen once in 3s with 2s interval.
  EXPECT_EQ(
      1,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
  regathering_controller()->CancelScheduledRegathering();
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect no regathering happened in the last 3s.
  EXPECT_EQ(
      1,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
  regathering_controller()->ScheduleRegatheringOnAllNetworks(
      false /* repeated */);
  const int kNetworkGatherDurationLong = 11000;
  // Expect regathering to happen only once in 11s.
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  EXPECT_EQ(
      2,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
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
  regathering_controller()->set_config(config);

  const int kNetworkGatherDurationShort = 3000;
  regathering_controller()->ScheduleRegatheringOnAllNetworks(
      true /* repeated*/);
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect regathering to happen once in 3s with 2s interval.
  EXPECT_EQ(
      1,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
  regathering_controller()->CancelScheduledRegathering();
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect no regathering happened in the last 3s.
  EXPECT_EQ(
      1,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
  regathering_controller()->ScheduleRegatheringOnAllNetworks(
      true /* repeated */);
  const int kNetworkGatherDurationLong = 11000;
  // Expect regathering to happen for another 5 times in 11s with 2s
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  EXPECT_EQ(
      6,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
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
  regathering_controller()->set_config(config);

  const int kNetworkGatherDurationShort = 3000;
  regathering_controller()->CancelScheduledRegathering();
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect no regathering happened in the last 3s after regathering is
  // canceled.
  EXPECT_EQ(
      0,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
  // Resume the regathering.
  regathering_controller()->ScheduleRegatheringOnAllNetworks(
      true /* repeated */);
  SIMULATED_WAIT(false, kRegatherIntervalShort - 500, clock);
  // Reschedule repeating regathering. We should not have regathered yet.
  const int kRegatherIntervalLong = 5000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  regathering_controller()->set_config(config);
  regathering_controller()->ScheduleRegatheringOnAllNetworks(
      true /* repeated */);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen twice in the last 13.5s with 5s interval.
  EXPECT_EQ(
      2,
      metric_observer()->GetEnumCounter(
          webrtc::kEnumCounterIceRegathering,
          static_cast<int>(cricket::IceRegatheringReason::OCCASIONAL_REFRESH)));
}

}  // namespace webrtc
