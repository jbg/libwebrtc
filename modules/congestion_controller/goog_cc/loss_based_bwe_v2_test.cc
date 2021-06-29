/*
 *  Copyright 2021 The WebRTC project authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"

#include <array>
#include <string>
#include <vector>

#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "rtc_base/strings/string_builder.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr DataSize k15Kilobytes = DataSize::Bytes(15000);
constexpr DataRate k600KilobitsPerSecond = DataRate::KilobitsPerSec(600);
constexpr int kObservationDurationLowerBoundInMs = 200;

std::string CreateConfigString(bool enabled, bool valid) {
  char buffer[1024];
  rtc::SimpleStringBuilder config_string(buffer);

  config_string << "WebRTC-Bwe-LossBasedBweV2/";

  if (enabled) {
    config_string << "Enabled:true";
  } else {
    config_string << "Enabled:false";
  }

  if (valid) {
    config_string << ",BwRampupUpperBoundFactor:1.2";
  } else {
    config_string << ",BwRampupUpperBoundFactor:0.0";
  }

  config_string
      << ",CandidateFactors:0.9|1.1,HigherBwBiasFactor:0.01,"
         "InherentLossLowerBound:0.001,InherentLossUpperBoundBwBalance:14kbps,"
         "InherentLossUpperBoundOffset:0.9,InitialInherentLossEstimate:0.01,"
         "NewtonIterations:2,NewtonStepSize:0.4,ObservationWindowSize:15,"
         "SendingRateSmoothingFactor:0.01,TcpFairnessTemporalWeightFactor:0.97,"
         "TcpFairnessUpperBoundBwBalance:90kbps,"
         "TcpFairnessUpperBoundLossOffset:0.1,TemporalWeightFactor:0.98";

  config_string.AppendFormat(",ObservationDurationLowerBound:%dms",
                             kObservationDurationLowerBoundInMs);

  config_string << "/";

  return config_string.str();
}

LossBasedBweV2 CreateLossBasedBweV2(const std::string& config_string) {
  test::ExplicitKeyValueConfig key_value_config =
      test::ExplicitKeyValueConfig(config_string);
  return LossBasedBweV2(&key_value_config);
}

PacketResult CreatePacketResult(Timestamp send_time,
                                Timestamp receive_time,
                                DataSize packet_size) {
  PacketResult packet_result = PacketResult();
  packet_result.receive_time = receive_time;
  packet_result.sent_packet.send_time = send_time;
  packet_result.sent_packet.size = packet_size;
  return packet_result;
}

}  // namespace

TEST(LossBasedBweV2Test, EnabledWhenGivenValidConfigurationValues) {
  bool enabled = true;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  EXPECT_TRUE(loss_based_bandwidth_estimator_v2.IsEnabled());
}

TEST(LossBasedBweV2Test, DisabledWhenGivenDisabledConfiguration) {
  bool enabled = false;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  EXPECT_FALSE(loss_based_bandwidth_estimator_v2.IsEnabled());
}

TEST(LossBasedBweV2Test, DisabledWhenGivenNonValidConfigurationValues) {
  bool enabled = true;
  bool valid = false;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  EXPECT_FALSE(loss_based_bandwidth_estimator_v2.IsEnabled());
}

TEST(LossBasedBweV2Test, BandwidthEstimateGivenInitializationAndThenFeedback) {
  PacketResult packet_result_1 = CreatePacketResult(
      Timestamp::Millis(0),
      Timestamp::Millis(kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_2 = CreatePacketResult(
      Timestamp::Millis(kObservationDurationLowerBoundInMs),
      Timestamp::Millis(2 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  std::vector<PacketResult> enough_feedback = {packet_result_1,
                                               packet_result_2};

  bool enabled = true;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  loss_based_bandwidth_estimator_v2.SetBandwidthEstimate(k600KilobitsPerSecond);
  loss_based_bandwidth_estimator_v2.UpdateBandwidthEstimate(enough_feedback);
  EXPECT_TRUE(loss_based_bandwidth_estimator_v2.IsReady());
  EXPECT_FALSE(loss_based_bandwidth_estimator_v2.GetBandwidthEstimate()
                   .IsPlusInfinity());
}

TEST(LossBasedBweV2Test, NoBandwidthEstimateGivenNoInitialization) {
  PacketResult packet_result_1 = CreatePacketResult(
      Timestamp::Millis(0),
      Timestamp::Millis(kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_2 = CreatePacketResult(
      Timestamp::Millis(kObservationDurationLowerBoundInMs),
      Timestamp::Millis(2 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  std::vector<PacketResult> enough_feedback = {packet_result_1,
                                               packet_result_2};

  bool enabled = true;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  loss_based_bandwidth_estimator_v2.UpdateBandwidthEstimate(enough_feedback);
  EXPECT_FALSE(loss_based_bandwidth_estimator_v2.IsReady());
  EXPECT_TRUE(loss_based_bandwidth_estimator_v2.GetBandwidthEstimate()
                  .IsPlusInfinity());
}

TEST(LossBasedBweV2Test, NoBandwidthEstimateGivenNotEnoughFeedback) {
  // Create packet results where the observation duration is less than the lower
  // bound.
  PacketResult packet_result_1 = CreatePacketResult(
      Timestamp::Millis(0),
      Timestamp::Millis(kObservationDurationLowerBoundInMs / 2), k15Kilobytes);
  PacketResult packet_result_2 = CreatePacketResult(
      Timestamp::Millis(kObservationDurationLowerBoundInMs / 2),
      Timestamp::Millis(kObservationDurationLowerBoundInMs), k15Kilobytes);
  std::vector<PacketResult> not_enough_feedback = {packet_result_1,
                                                   packet_result_2};

  bool enabled = true;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  loss_based_bandwidth_estimator_v2.SetBandwidthEstimate(k600KilobitsPerSecond);
  EXPECT_FALSE(loss_based_bandwidth_estimator_v2.IsReady());
  EXPECT_TRUE(loss_based_bandwidth_estimator_v2.GetBandwidthEstimate()
                  .IsPlusInfinity());
  loss_based_bandwidth_estimator_v2.UpdateBandwidthEstimate(
      not_enough_feedback);
  EXPECT_FALSE(loss_based_bandwidth_estimator_v2.IsReady());
  EXPECT_TRUE(loss_based_bandwidth_estimator_v2.GetBandwidthEstimate()
                  .IsPlusInfinity());
}

TEST(LossBasedBweV2Test,
     SetValueIsTheEstimateUntilAdditionalFeedbackHasBeenReceived) {
  PacketResult packet_result_1 = CreatePacketResult(
      Timestamp::Millis(0),
      Timestamp::Millis(kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_2 = CreatePacketResult(
      Timestamp::Millis(kObservationDurationLowerBoundInMs),
      Timestamp::Millis(2 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_3 = CreatePacketResult(
      Timestamp::Millis(2 * kObservationDurationLowerBoundInMs),
      Timestamp::Millis(3 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_4 = CreatePacketResult(
      Timestamp::Millis(3 * kObservationDurationLowerBoundInMs),
      Timestamp::Millis(4 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  std::vector<PacketResult> enough_feedback_1 = {packet_result_1,
                                                 packet_result_2};
  std::vector<PacketResult> enough_feedback_2 = {packet_result_3,
                                                 packet_result_4};

  bool enabled = true;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  loss_based_bandwidth_estimator_v2.SetBandwidthEstimate(k600KilobitsPerSecond);
  loss_based_bandwidth_estimator_v2.UpdateBandwidthEstimate(enough_feedback_1);
  EXPECT_NE(loss_based_bandwidth_estimator_v2.GetBandwidthEstimate(),
            k600KilobitsPerSecond);
  loss_based_bandwidth_estimator_v2.SetBandwidthEstimate(k600KilobitsPerSecond);
  EXPECT_EQ(loss_based_bandwidth_estimator_v2.GetBandwidthEstimate(),
            k600KilobitsPerSecond);
  loss_based_bandwidth_estimator_v2.UpdateBandwidthEstimate(enough_feedback_2);
  EXPECT_NE(loss_based_bandwidth_estimator_v2.GetBandwidthEstimate(),
            k600KilobitsPerSecond);
}

TEST(LossBasedBweV2Test,
     SetAcknowledgedBitrateOnlyAffectsTheBweWhenAdditionalFeedbackIsGiven) {
  PacketResult packet_result_1 = CreatePacketResult(
      Timestamp::Millis(0),
      Timestamp::Millis(kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_2 = CreatePacketResult(
      Timestamp::Millis(kObservationDurationLowerBoundInMs),
      Timestamp::Millis(2 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_3 = CreatePacketResult(
      Timestamp::Millis(2 * kObservationDurationLowerBoundInMs),
      Timestamp::Millis(3 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  PacketResult packet_result_4 = CreatePacketResult(
      Timestamp::Millis(3 * kObservationDurationLowerBoundInMs),
      Timestamp::Millis(4 * kObservationDurationLowerBoundInMs), k15Kilobytes);
  std::vector<PacketResult> enough_feedback_1 = {packet_result_1,
                                                 packet_result_2};
  std::vector<PacketResult> enough_feedback_2 = {packet_result_3,
                                                 packet_result_4};

  bool enabled = true;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2_1 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  LossBasedBweV2 loss_based_bandwidth_estimator_v2_2 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  loss_based_bandwidth_estimator_v2_1.SetBandwidthEstimate(
      k600KilobitsPerSecond);
  loss_based_bandwidth_estimator_v2_2.SetBandwidthEstimate(
      k600KilobitsPerSecond);
  loss_based_bandwidth_estimator_v2_1.UpdateBandwidthEstimate(
      enough_feedback_1);
  loss_based_bandwidth_estimator_v2_2.UpdateBandwidthEstimate(
      enough_feedback_1);
  EXPECT_EQ(loss_based_bandwidth_estimator_v2_1.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_v2_1.SetAcknowledgedBitrate(
      k600KilobitsPerSecond);
  EXPECT_EQ(loss_based_bandwidth_estimator_v2_1.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_v2_1.UpdateBandwidthEstimate(
      enough_feedback_2);
  loss_based_bandwidth_estimator_v2_2.UpdateBandwidthEstimate(
      enough_feedback_2);
  EXPECT_NE(loss_based_bandwidth_estimator_v2_1.GetBandwidthEstimate(),
            loss_based_bandwidth_estimator_v2_2.GetBandwidthEstimate());
}

TEST(LossBasedBweV2Test,
     BandwidthEstimateIsCappedToBeTcpFairGivenTooHighLossRate) {
  PacketResult packet_result_1 = CreatePacketResult(
      Timestamp::Millis(0), Timestamp::PlusInfinity(), k15Kilobytes);
  PacketResult packet_result_2 =
      CreatePacketResult(Timestamp::Millis(kObservationDurationLowerBoundInMs),
                         Timestamp::PlusInfinity(), k15Kilobytes);
  std::vector<PacketResult> enough_feedback_no_received_packets = {
      packet_result_1, packet_result_2};

  bool enabled = true;
  bool valid = true;
  LossBasedBweV2 loss_based_bandwidth_estimator_v2_1 =
      CreateLossBasedBweV2(CreateConfigString(enabled, valid));
  loss_based_bandwidth_estimator_v2_1.SetBandwidthEstimate(
      k600KilobitsPerSecond);
  loss_based_bandwidth_estimator_v2_1.UpdateBandwidthEstimate(
      enough_feedback_no_received_packets);
  EXPECT_EQ(loss_based_bandwidth_estimator_v2_1.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(100));
}

}  // namespace webrtc
