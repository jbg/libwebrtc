/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/include/receive_side_congestion_controller.h"

#include "api/media_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_layer_feedback.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
static const uint32_t kTimeOffsetSwitchThreshold = 30;
}  // namespace

void ReceiveSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                                  int64_t max_rtt_ms) {
  MutexLock lock(&mutex_);
  rbe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

void ReceiveSideCongestionController::RemoveStream(uint32_t ssrc) {
  MutexLock lock(&mutex_);
  rbe_->RemoveStream(ssrc);
}

DataRate ReceiveSideCongestionController::LatestReceiveSideEstimate() const {
  MutexLock lock(&mutex_);
  return rbe_->LatestEstimate();
}

void ReceiveSideCongestionController::PickEstimator(
    bool has_absolute_send_time) {
  if (has_absolute_send_time) {
    // If we see AST in header, switch RBE strategy immediately.
    if (!using_absolute_send_time_) {
      RTC_LOG(LS_INFO)
          << "WrappingBitrateEstimator: Switching to absolute send time RBE.";
      using_absolute_send_time_ = true;
      rbe_ = std::make_unique<RemoteBitrateEstimatorAbsSendTime>(
          &remb_throttler_, &clock_);
    }
    packets_since_absolute_send_time_ = 0;
  } else {
    // When we don't see AST, wait for a few packets before going back to TOF.
    if (using_absolute_send_time_) {
      ++packets_since_absolute_send_time_;
      if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
        RTC_LOG(LS_INFO)
            << "WrappingBitrateEstimator: Switching to transmission "
               "time offset RBE.";
        using_absolute_send_time_ = false;
        rbe_ = std::make_unique<RemoteBitrateEstimatorSingleStream>(
            &remb_throttler_, &clock_);
      }
    }
  }
}

ReceiveSideCongestionController::ReceiveSideCongestionController(
    Clock* clock,
    RemoteEstimatorProxy::TransportFeedbackSender feedback_sender,
    RembThrottler::RembSender remb_sender,
    NetworkStateEstimator* network_state_estimator)
    : ReceiveSideCongestionController(
          clock,
          std::move(feedback_sender),
          std::move(remb_sender),
          network_state_estimator,
          /*support_rfc8888_feedback_format=*/false) {}

ReceiveSideCongestionController::ReceiveSideCongestionController(
    Clock* clock,
    RemoteEstimatorProxy::TransportFeedbackSender feedback_sender,
    RembThrottler::RembSender remb_sender,
    NetworkStateEstimator* network_state_estimator,
    bool support_rfc8888_feedback_format)
    : clock_(*clock),
      support_rfc8888_feedback_format_(support_rfc8888_feedback_format),
      feedback_sender_(*clock, feedback_sender),
      remb_throttler_(std::move(remb_sender), clock),
      remote_estimator_proxy_(feedback_sender, network_state_estimator),
      rbe_(new RemoteBitrateEstimatorSingleStream(&remb_throttler_, clock)),
      using_absolute_send_time_(false),
      packets_since_absolute_send_time_(0) {}

void ReceiveSideCongestionController::OnReceivedPacket(
    const RtpPacketReceived& packet,
    MediaType media_type) {
  bool has_transport_sequence_number =
      packet.HasExtension<TransportSequenceNumber>() ||
      packet.HasExtension<TransportSequenceNumberV2>();
  if (media_type == MediaType::AUDIO && !has_transport_sequence_number &&
      !support_rfc8888_feedback_format_) {
    // For audio, we only support send side BWE.
    return;
  }

  if (has_transport_sequence_number) {
    // Send-side BWE.
    remote_estimator_proxy_.IncomingPacket(packet);
  } else {
    if (support_rfc8888_feedback_format_) {
      feedback_sender_.OnReceivedPacket(packet);
    }
    // Receive-side BWE.
    MutexLock lock(&mutex_);
    PickEstimator(packet.HasExtension<AbsoluteSendTime>());
    rbe_->IncomingPacket(packet);
  }
}

void ReceiveSideCongestionController::OnBitrateChanged(int bitrate_bps) {
  remote_estimator_proxy_.OnBitrateChanged(bitrate_bps);
  if (support_rfc8888_feedback_format_) {
    feedback_sender_.OnTargetBitrateChanged(DataRate::BitsPerSec(bitrate_bps));
  }
}

TimeDelta ReceiveSideCongestionController::MaybeProcess() {
  Timestamp now = clock_.CurrentTime();
  mutex_.Lock();
  TimeDelta time_until_rbe = rbe_->Process();
  mutex_.Unlock();
  TimeDelta time_until_rep = remote_estimator_proxy_.Process(now);
  TimeDelta time_until_feedback = support_rfc8888_feedback_format_
                                      ? feedback_sender_.Process(now)
                                      : TimeDelta::PlusInfinity();
  TimeDelta time_until =
      std::min({time_until_rbe, time_until_rep, time_until_feedback});
  return std::max(time_until, TimeDelta::Zero());
}

void ReceiveSideCongestionController::SetMaxDesiredReceiveBitrate(
    DataRate bitrate) {
  remb_throttler_.SetMaxDesiredReceiveBitrate(bitrate);
}

void ReceiveSideCongestionController::SetTransportOverhead(
    DataSize overhead_per_packet) {
  remote_estimator_proxy_.SetTransportOverhead(overhead_per_packet);
  if (support_rfc8888_feedback_format_) {
    feedback_sender_.SetTransportOverhead(overhead_per_packet);
  }
}

}  // namespace webrtc
