/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <limits>

#include "modules/remote_bitrate_estimator/remote_estimator_proxy.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Return;

namespace webrtc {
namespace {

constexpr size_t kDefaultPacketSize = 100;
constexpr uint32_t kMediaSsrc = 456;
constexpr uint16_t kBaseSeq = 10;
constexpr int64_t kBaseTimeMs = 123;
constexpr int64_t kMaxSmallDeltaMs =
    (rtcp::TransportFeedback::kDeltaScaleFactor * 0xFF) / 1000;

constexpr TransportFeedbackConfig kNoFeedbackConfig = {
    /*on_request=*/true, /*include_timestamps=*/true, /*sequence_count=*/0};
constexpr TransportFeedbackConfig kSinglePacketFeedbackConfig = {
    /*on_request=*/true, /*include_timestamps=*/true, /*sequence_count=*/1};
constexpr TransportFeedbackConfig kFivePacketsFeedbackConfig = {
    /*on_request=*/true, /*include_timestamps=*/true, /*sequence_count=*/5};

std::vector<uint16_t> SequenceNumbers(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<uint16_t> sequence_numbers;
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    sequence_numbers.push_back(rtp_packet_received.sequence_number());
  }
  return sequence_numbers;
}

std::vector<int64_t> TimestampsMs(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<int64_t> timestamps;
  int64_t timestamp_us = feedback_packet.GetBaseTimeUs();
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    timestamp_us += rtp_packet_received.delta_us();
    timestamps.push_back(timestamp_us / 1000);
  }
  return timestamps;
}

class MockTransportFeedbackSender : public TransportFeedbackSenderInterface {
 public:
  MOCK_METHOD1(SendTransportFeedback,
               bool(rtcp::TransportFeedback* feedback_packet));
};

class RemoteEstimatorProxyTest : public ::testing::Test {
 public:
  RemoteEstimatorProxyTest() : clock_(0), proxy_(&clock_, &router_) {}

 protected:
  void IncomingPacket(uint16_t seq,
                      int64_t time_ms,
                      TransportFeedbackConfig feedback_config) {
    RTPHeader header;
    header.extension.hasTransportSequenceNumber = true;
    header.extension.transportSequenceNumber = seq;
    header.extension.feedback_config = feedback_config;
    header.ssrc = kMediaSsrc;
    proxy_.IncomingPacket(time_ms, kDefaultPacketSize, header);
  }

  void IncomingPacket(uint16_t seq, int64_t time_ms) {
    TransportFeedbackConfig feedback_config = {false, true, 0};
    IncomingPacket(seq, time_ms, feedback_config);
  }

  void Process() {
    clock_.AdvanceTimeMilliseconds(
        RemoteEstimatorProxy::kDefaultSendIntervalMs);
    proxy_.Process();
  }

  SimulatedClock clock_;
  testing::StrictMock<MockTransportFeedbackSender> router_;
  RemoteEstimatorProxy proxy_;
};

TEST_F(RemoteEstimatorProxyTest, SendsSinglePacketFeedback) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet), ElementsAre(kBaseSeq));
        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, DuplicatedPackets) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq, kBaseTimeMs + 1000);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet), ElementsAre(kBaseSeq));
        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, FeedbackWithMissingStart) {
  // First feedback.
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + 1000);
  EXPECT_CALL(router_, SendTransportFeedback(_)).WillOnce(Return(true));
  Process();

  // Second feedback starts with a missing packet (DROP kBaseSeq + 2).
  IncomingPacket(kBaseSeq + 3, kBaseTimeMs + 3000);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 3));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 3000));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFeedbackWithVaryingDeltas) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kMaxSmallDeltaMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + (2 * kMaxSmallDeltaMs) + 1);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 2));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs, kBaseTimeMs + kMaxSmallDeltaMs,
                                kBaseTimeMs + (2 * kMaxSmallDeltaMs) + 1));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFragmentedFeedback) {
  static constexpr int64_t kTooLargeDelta =
      rtcp::TransportFeedback::kDeltaScaleFactor * (1 << 16);

  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kTooLargeDelta);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet), ElementsAre(kBaseSeq));
        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 1));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + kTooLargeDelta));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, GracefullyHandlesReorderingAndWrap) {
  const int64_t kDeltaMs = 1000;
  const uint16_t kLargeSeq = 62762;
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kLargeSeq, kBaseTimeMs + kDeltaMs);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, ResendsTimestampsOnReordering) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + 2);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq, kBaseSeq + 2));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs, kBaseTimeMs + 2));
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + 1);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 1, kBaseSeq + 2));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 1, kBaseTimeMs + 2));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, RemovesTimestampsOutOfScope) {
  const int64_t kTimeoutTimeMs =
      kBaseTimeMs + RemoteEstimatorProxy::kBackWindowMs;

  IncomingPacket(kBaseSeq + 2, kBaseTimeMs);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());

        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 3, kTimeoutTimeMs);  // kBaseSeq + 2 times out here.

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(
          Invoke([kTimeoutTimeMs](rtcp::TransportFeedback* feedback_packet) {
            EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());

            EXPECT_THAT(TimestampsMs(*feedback_packet),
                        ElementsAre(kTimeoutTimeMs));
            return true;
          }));

  Process();

  // New group, with sequence starting below the first so that they may be
  // retransmitted.
  IncomingPacket(kBaseSeq, kBaseTimeMs - 1);
  IncomingPacket(kBaseSeq + 1, kTimeoutTimeMs - 1);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(
          Invoke([kTimeoutTimeMs](rtcp::TransportFeedback* feedback_packet) {
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 3));
            EXPECT_THAT(TimestampsMs(*feedback_packet),
                        ElementsAre(kBaseTimeMs - 1, kTimeoutTimeMs - 1,
                                    kTimeoutTimeMs));
            return true;
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsZeroBeforeFirstProcess) {
  EXPECT_EQ(0, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsDefaultOnUnkownBitrate) {
  Process();
  EXPECT_EQ(RemoteEstimatorProxy::kDefaultSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMinIntervalOn300kbps) {
  Process();
  proxy_.OnBitrateChanged(300000);
  EXPECT_EQ(RemoteEstimatorProxy::kMinSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn0kbps) {
  Process();
  // TimeUntilNextProcess should be limited by |kMaxSendIntervalMs| when
  // bitrate is small. We choose 0 bps as a special case, which also tests
  // erroneous behaviors like division-by-zero.
  proxy_.OnBitrateChanged(0);
  EXPECT_EQ(RemoteEstimatorProxy::kMaxSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn20kbps) {
  Process();
  proxy_.OnBitrateChanged(20000);
  EXPECT_EQ(RemoteEstimatorProxy::kMaxSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TwccReportsUse5PercentOfAvailableBandwidth) {
  Process();
  proxy_.OnBitrateChanged(80000);
  // 80kbps * 0.05 = TwccReportSize(68B * 8b/B) * 1000ms / SendInterval(136ms)
  EXPECT_EQ(136, proxy_.TimeUntilNextProcess());
}

//////////////////////////////////////////////////////////////////////////////
// Tests for the extended protocol where the feedback is explicitly requested
// by the sender.
//////////////////////////////////////////////////////////////////////////////
typedef RemoteEstimatorProxyTest RemoteEstimatorProxyOnRequestTest;
TEST_F(RemoteEstimatorProxyOnRequestTest,
       TimeUntilNextProcessIsHighWhenFeedbackQueueEmpty) {
  IncomingPacket(kBaseSeq, kBaseTimeMs, kNoFeedbackConfig);
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyOnRequestTest,
       TimeUntilNextProcessIsZeroWhenFeedbackQueueNonEmpty) {
  IncomingPacket(kBaseSeq, kBaseTimeMs, kSinglePacketFeedbackConfig);
  EXPECT_EQ(0, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyOnRequestTest,
       TimeUntilNextProcessIsHighAfterFeedbackQueueIsProcessed) {
  IncomingPacket(kBaseSeq, kBaseTimeMs, kSinglePacketFeedbackConfig);
  EXPECT_CALL(router_, SendTransportFeedback(_)).WillOnce(Return(true));
  Process();
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestSinglePacketFeedback) {
  IncomingPacket(kBaseSeq, kBaseTimeMs, kNoFeedbackConfig);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kMaxSmallDeltaMs,
                 kNoFeedbackConfig);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + 2 * kMaxSmallDeltaMs,
                 kNoFeedbackConfig);
  IncomingPacket(kBaseSeq + 3, kBaseTimeMs + 3 * kMaxSmallDeltaMs,
                 kSinglePacketFeedbackConfig);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 3));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 3 * kMaxSmallDeltaMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestLastFivePacketFeedback) {
  int i = 0;
  for (; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs,
                   kNoFeedbackConfig);
  }
  IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs,
                 kFivePacketsFeedbackConfig);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 6, kBaseSeq + 7, kBaseSeq + 8,
                                kBaseSeq + 9, kBaseSeq + 10));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 6 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 7 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 8 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 9 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 10 * kMaxSmallDeltaMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyOnRequestTest,
       RequestLastFivePacketFeedbackMissingPackets) {
  int i = 0;
  for (; i < 10; ++i) {
    if (i != 7 && i != 9)
      IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs,
                     kNoFeedbackConfig);
  }
  IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs,
                 kFivePacketsFeedbackConfig);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 6, kBaseSeq + 8, kBaseSeq + 10));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 6 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 8 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 10 * kMaxSmallDeltaMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyOnRequestTest, AllFeedbackRequestsAreProcessed) {
  int i = 0;
  for (; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs,
                   kNoFeedbackConfig);
  }

  for (int j = 0; j < 3; ++j) {
    IncomingPacket(kBaseSeq + i + j, kBaseTimeMs + (i + j) * kMaxSmallDeltaMs,
                   kSinglePacketFeedbackConfig);
  }

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([i](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + i, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + i));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + i * kMaxSmallDeltaMs));
        return true;
      }))
      .WillOnce(Invoke([i](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + i + 1, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + i + 1));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + (i + 1) * kMaxSmallDeltaMs));
        return true;
      }))
      .WillOnce(Invoke([i](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + i + 2, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + i + 2));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + (i + 2) * kMaxSmallDeltaMs));
        return true;
      }));

  Process();
  // Queue must be empty now.
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), proxy_.TimeUntilNextProcess());
}

}  // namespace
}  // namespace webrtc
