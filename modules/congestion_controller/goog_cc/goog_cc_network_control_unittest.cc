/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/congestion_controller/goog_cc/include/goog_cc_factory.h"
#include "modules/congestion_controller/network_control/test/mock_network_control.h"
#include "modules/congestion_controller/network_control/test/network_control_tester.h"

#include "test/gtest.h"

using testing::Field;
using testing::Matcher;
using testing::NiceMock;
using testing::Property;
using testing::StrictMock;
using testing::_;

namespace webrtc {
namespace webrtc_cc {
namespace test {
namespace {

const uint32_t kInitialBitrateKbps = 60000;
const DataRate kInitialBitrate = DataRate::kbps(kInitialBitrateKbps);
const float kDefaultPacingRate = 2.5f;

inline Matcher<TargetTransferRate> TargetRateEq(DataRate rate) {
  return Field(&TargetTransferRate::target_rate, rate);
}
inline Matcher<PacerConfig> PacingRateEq(DataRate rate) {
  return Property(&PacerConfig::data_rate, rate);
}
inline Matcher<ProbeClusterConfig> ProbeRateEq(DataRate rate) {
  return Field(&ProbeClusterConfig::target_data_rate, rate);
}

}  // namespace

class GoogCcNetworkControllerTest : public ::testing::Test {
 protected:
  GoogCcNetworkControllerTest()
      : current_time_(Timestamp::ms(123456)), target_bitrate_observer_() {}
  ~GoogCcNetworkControllerTest() override {}

  void SetUp() override {
    // Set the initial bitrate estimate and expect the |observer|
    // to be updated.
    EXPECT_CALL(observer_, OnTargetTransferRate(TargetRateEq(kInitialBitrate)));
    EXPECT_CALL(observer_, OnPacerConfig(PacingRateEq(kInitialBitrate *
                                                      kDefaultPacingRate)));
    EXPECT_CALL(observer_,
                OnProbeClusterConfig(ProbeRateEq(kInitialBitrate * 3)));
    EXPECT_CALL(observer_,
                OnProbeClusterConfig(ProbeRateEq(kInitialBitrate * 5)));

    controller_.reset(
        new GoogCcNetworkController(&event_log_, &observer_, InitialConfig()));
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }
  // Custom setup - use an observer that tracks the target bitrate, without
  // prescribing on which iterations it must change (like a mock would).
  void TargetBitrateTrackingSetup() {
    controller_.reset(new GoogCcNetworkController(
        &event_log_, &target_bitrate_observer_, InitialConfig()));
  }

  NetworkControllerConfig InitialConfig(
      int starting_bandwidth_kbps = kInitialBitrateKbps,
      int min_data_rate_kbps = 0,
      int max_data_rate_kbps = 5 * kInitialBitrateKbps) {
    NetworkControllerConfig config;
    config.constraints.at_time = current_time_;
    config.constraints.min_data_rate = DataRate::kbps(min_data_rate_kbps);
    config.constraints.max_data_rate = DataRate::kbps(max_data_rate_kbps);
    config.starting_bandwidth = DataRate::kbps(starting_bandwidth_kbps);
    return config;
  }
  ProcessInterval DefaultInterval() {
    ProcessInterval interval;
    interval.at_time = current_time_;
    return interval;
  }
  RemoteBitrateReport CreateBitrateReport(DataRate rate) {
    RemoteBitrateReport report;
    report.receive_time = current_time_;
    report.bandwidth = rate;
    return report;
  }
  PacketResult CreateResult(int64_t arrival_time_ms,
                            int64_t send_time_ms,
                            size_t payload_size,
                            PacedPacketInfo pacing_info) {
    PacketResult packet_result;
    packet_result.sent_packet = SentPacket();
    packet_result.sent_packet->send_time = Timestamp::ms(send_time_ms);
    packet_result.sent_packet->size = DataSize::bytes(payload_size);
    packet_result.sent_packet->pacing_info = pacing_info;
    packet_result.receive_time = Timestamp::ms(arrival_time_ms);
    return packet_result;
  }

  NetworkRouteChange CreateRouteChange(
      DataRate start_rate = DataRate::kNotInitialized,
      DataRate min_rate = DataRate::kNotInitialized,
      DataRate max_rate = DataRate::kNotInitialized) {
    NetworkRouteChange route_change;
    route_change.at_time = current_time_;
    route_change.constraints.at_time = current_time_;
    route_change.constraints.min_data_rate = min_rate;
    route_change.constraints.max_data_rate = max_rate;
    route_change.starting_rate = start_rate;
    return route_change;
  }

  void AdvanceTimeMilliseconds(int timedelta_ms) {
    current_time_ += TimeDelta::ms(timedelta_ms);
  }

  // Allows us to track the target bitrate, without prescribing the exact
  // iterations when this would hapen, like a mock would.
  class TargetBitrateObserver : public NetworkControllerObserver {
   public:
    TargetBitrateObserver() = default;
    ~TargetBitrateObserver() override = default;
    void OnCongestionWindow(CongestionWindow) override {}
    void OnPacerConfig(PacerConfig) override {}
    void OnProbeClusterConfig(ProbeClusterConfig) override {}
    void OnTargetTransferRate(TargetTransferRate msg) override {
      target_bitrate_ = msg.target_rate;
    }
    rtc::Optional<DataRate> target_bitrate_;
  };

  void PacketTransmissionAndFeedbackBlock(int64_t runtime_ms, int64_t delay) {
    int64_t delay_buildup = 0;
    int64_t start_time_ms = current_time_.ms();
    while (current_time_.ms() - start_time_ms < runtime_ms) {
      constexpr size_t kPayloadSize = 1000;
      PacketResult packet =
          CreateResult(current_time_.ms() + delay_buildup, current_time_.ms(),
                       kPayloadSize, PacedPacketInfo());
      delay_buildup += delay;  // Delay has to increase, or it's just RTT.
      controller_->OnSentPacket(*packet.sent_packet);
      TransportPacketsFeedback feedback;
      feedback.feedback_time = packet.receive_time;
      feedback.packet_feedbacks.push_back(packet);
      controller_->OnTransportPacketsFeedback(feedback);
      AdvanceTimeMilliseconds(50);
      controller_->OnProcessInterval(DefaultInterval());
    }
  }
  Timestamp current_time_;
  StrictMock<webrtc::test::MockNetworkControllerObserver> observer_;
  TargetBitrateObserver target_bitrate_observer_;
  NiceMock<MockRtcEventLog> event_log_;
  std::unique_ptr<NetworkControllerInterface> controller_;
};

TEST_F(GoogCcNetworkControllerTest, OnNetworkChanged) {
  // Test no change.
  AdvanceTimeMilliseconds(25);
  controller_->OnProcessInterval(DefaultInterval());

  EXPECT_CALL(observer_,
              OnTargetTransferRate(TargetRateEq(kInitialBitrate * 2)));
  EXPECT_CALL(observer_, OnPacerConfig(PacingRateEq(kInitialBitrate * 2 *
                                                    kDefaultPacingRate)));

  controller_->OnRemoteBitrateReport(CreateBitrateReport(kInitialBitrate * 2));
  AdvanceTimeMilliseconds(25);
  controller_->OnProcessInterval(DefaultInterval());

  EXPECT_CALL(observer_, OnTargetTransferRate(TargetRateEq(kInitialBitrate)));
  EXPECT_CALL(observer_, OnPacerConfig(PacingRateEq(kInitialBitrate *
                                                    kDefaultPacingRate)));
  controller_->OnRemoteBitrateReport(CreateBitrateReport(kInitialBitrate));
  AdvanceTimeMilliseconds(25);
  controller_->OnProcessInterval(DefaultInterval());
}

TEST_F(GoogCcNetworkControllerTest, OnNetworkRouteChanged) {
  DataRate new_bitrate = DataRate::bps(200000);
  EXPECT_CALL(observer_, OnTargetTransferRate(TargetRateEq(new_bitrate)));
  EXPECT_CALL(observer_,
              OnPacerConfig(PacingRateEq(new_bitrate * kDefaultPacingRate)));

  EXPECT_CALL(observer_, OnProbeClusterConfig(_)).Times(2);
  controller_->OnNetworkRouteChange(CreateRouteChange(new_bitrate));

  testing::Mock::VerifyAndClearExpectations(&observer_);
  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate kMinBitrateBps.
  const DataRate kDefaultMinBitrate =
      DataRate::bps(congestion_controller::GetMinBitrateBps());
  EXPECT_CALL(observer_,
              OnTargetTransferRate(TargetRateEq(kDefaultMinBitrate)));
  EXPECT_CALL(observer_, OnPacerConfig(PacingRateEq(kDefaultMinBitrate *
                                                    kDefaultPacingRate)));
  EXPECT_CALL(observer_, OnProbeClusterConfig(_)).Times(2);
  controller_->OnNetworkRouteChange(CreateRouteChange());
}

TEST_F(GoogCcNetworkControllerTest, ProbeOnRouteChange) {
  EXPECT_CALL(observer_,
              OnProbeClusterConfig(ProbeRateEq(kInitialBitrate * 6)));
  EXPECT_CALL(observer_,
              OnProbeClusterConfig(ProbeRateEq(kInitialBitrate * 12)));
  EXPECT_CALL(observer_, OnPacerConfig(_));
  EXPECT_CALL(observer_,
              OnTargetTransferRate(TargetRateEq(kInitialBitrate * 2)));
  controller_->OnNetworkRouteChange(CreateRouteChange(
      2 * kInitialBitrate, DataRate::Zero(), 20 * kInitialBitrate));
}

// Estimated bitrate reduced when the feedbacks arrive with such a long delay,
// that the send-time-history no longer holds the feedbacked packets.
TEST_F(GoogCcNetworkControllerTest, LongFeedbackDelays) {
  TargetBitrateTrackingSetup();
  const webrtc::PacedPacketInfo kPacingInfo0(0, 5, 2000);
  const webrtc::PacedPacketInfo kPacingInfo1(1, 8, 4000);
  const int64_t kFeedbackTimeoutMs = 60001;
  const int kMaxConsecutiveFailedLookups = 5;
  for (int i = 0; i < kMaxConsecutiveFailedLookups; ++i) {
    std::vector<PacketResult> packets;
    packets.push_back(CreateResult(i * 100, 2 * i * 100, 1500, kPacingInfo0));
    packets.push_back(
        CreateResult(i * 100 + 10, 2 * i * 100 + 10, 1500, kPacingInfo0));
    packets.push_back(
        CreateResult(i * 100 + 20, 2 * i * 100 + 20, 1500, kPacingInfo0));
    packets.push_back(
        CreateResult(i * 100 + 30, 2 * i * 100 + 30, 1500, kPacingInfo1));
    packets.push_back(
        CreateResult(i * 100 + 40, 2 * i * 100 + 40, 1500, kPacingInfo1));

    for (PacketResult& packet : packets) {
      controller_->OnSentPacket(*packet.sent_packet);
      // Simulate packet timeout
      packet.sent_packet = rtc::nullopt;
    }

    TransportPacketsFeedback feedback;
    feedback.feedback_time = packets[0].receive_time;
    feedback.packet_feedbacks = packets;

    AdvanceTimeMilliseconds(kFeedbackTimeoutMs);
    SentPacket later_packet;
    later_packet.send_time = Timestamp::ms(kFeedbackTimeoutMs + i * 200 + 40);
    later_packet.size = DataSize::bytes(1500);
    later_packet.pacing_info = kPacingInfo1;
    controller_->OnSentPacket(later_packet);

    controller_->OnTransportPacketsFeedback(feedback);
  }
  controller_->OnProcessInterval(DefaultInterval());

  EXPECT_EQ(kInitialBitrateKbps / 2,
            target_bitrate_observer_.target_bitrate_->kbps());

  // Test with feedback that isn't late enough to time out.
  {
    std::vector<PacketResult> packets;
    packets.push_back(CreateResult(100, 200, 1500, kPacingInfo0));
    packets.push_back(CreateResult(110, 210, 1500, kPacingInfo0));
    packets.push_back(CreateResult(120, 220, 1500, kPacingInfo0));
    packets.push_back(CreateResult(130, 230, 1500, kPacingInfo1));
    packets.push_back(CreateResult(140, 240, 1500, kPacingInfo1));

    for (const PacketResult& packet : packets)
      controller_->OnSentPacket(*packet.sent_packet);

    TransportPacketsFeedback feedback;
    feedback.feedback_time = packets[0].receive_time;
    feedback.packet_feedbacks = packets;

    AdvanceTimeMilliseconds(kFeedbackTimeoutMs - 1);

    SentPacket later_packet;
    later_packet.send_time = Timestamp::ms(kFeedbackTimeoutMs + 240);
    later_packet.size = DataSize::bytes(1500);
    later_packet.pacing_info = kPacingInfo1;
    controller_->OnSentPacket(later_packet);

    controller_->OnTransportPacketsFeedback(feedback);
  }
}

// Bandwidth estimation is updated when feedbacks are received.
// Feedbacks which show an increasing delay cause the estimation to be reduced.
TEST_F(GoogCcNetworkControllerTest, UpdatesDelayBasedEstimate) {
  TargetBitrateTrackingSetup();
  const int64_t kRunTimeMs = 6000;

  // The test must run and insert packets/feedback long enough that the
  // BWE computes a valid estimate. This is first done in an environment which
  // simulates no bandwidth limitation, and therefore not built-up delay.
  PacketTransmissionAndFeedbackBlock(kRunTimeMs, 0);
  ASSERT_TRUE(target_bitrate_observer_.target_bitrate_.has_value());

  // Repeat, but this time with a building delay, and make sure that the
  // estimation is adjusted downwards.
  DataRate bitrate_before_delay = *target_bitrate_observer_.target_bitrate_;
  PacketTransmissionAndFeedbackBlock(kRunTimeMs, 50);
  EXPECT_LT(*target_bitrate_observer_.target_bitrate_, bitrate_before_delay);
}

TEST_F(GoogCcNetworkControllerTest, UpdatesTargetSendRateBasedOnFeedback) {
  constexpr double kDataRateMargin = 0.2;
  constexpr double kMinDataRateFactor = 1 - kDataRateMargin;
  constexpr double kMaxDataRateFactor = 1 + kDataRateMargin;
  GoogCcNetworkControllerFactory factory(&event_log_);
  webrtc::test::FeedbackBasedNetworkControllerTester tester(
      &factory, InitialConfig(60, 0, 600));
  auto packet_producer = &webrtc::test::SimpleTargetRateProducer::ProduceNext;

  tester.RunSimulation(TimeDelta::seconds(5), TimeDelta::ms(10),
                       DataRate::kbps(300), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(300) * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(300) * kMaxDataRateFactor);

  tester.RunSimulation(TimeDelta::seconds(10), TimeDelta::ms(10),
                       DataRate::kbps(500), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(500) * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(500) * kMaxDataRateFactor);

  tester.RunSimulation(TimeDelta::seconds(20), TimeDelta::ms(10),
                       DataRate::kbps(100), TimeDelta::ms(200),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(100) * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate,
            DataRate::kbps(100) * kMaxDataRateFactor);
}

}  // namespace test
}  // namespace webrtc_cc
}  // namespace webrtc
