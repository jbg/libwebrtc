/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  FEC and NACK added bitrate is handled outside class
 */

#ifndef MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <deque>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RtcEventLog;

class SendSideBandwidthEstimation {
 public:
  SendSideBandwidthEstimation() = delete;
  explicit SendSideBandwidthEstimation(RtcEventLog* event_log);
  virtual ~SendSideBandwidthEstimation();

  void CurrentEstimate(int* bitrate, uint8_t* loss, int64_t* rtt) const;

  // Call periodically to update estimate.
  void UpdateEstimate(Timestamp now_ms);

  // Call when we receive a RTCP message with TMMBR or REMB.
  void UpdateReceiverEstimate(Timestamp now_ms, DataRate bandwidth);

  // Call when a new delay-based estimate is available.
  void UpdateDelayBasedEstimate(Timestamp now_ms, DataRate bitrate_bps);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdateReceiverBlock(uint8_t fraction_loss,
                           TimeDelta rtt_ms,
                           int number_of_packets,
                           Timestamp now_ms);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdatePacketsLost(int packets_lost,
                         int number_of_packets,
                         Timestamp now_ms);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdateRtt(TimeDelta rtt, Timestamp now_ms);

  void SetBitrates(absl::optional<DataRate> send_bitrate,
                   DataRate min_bitrate,
                   DataRate max_bitrate,
                   Timestamp now_ms);
  void SetSendBitrate(DataRate bitrate, Timestamp now_ms);
  void SetMinMaxBitrate(DataRate min_bitrate, DataRate max_bitrate);
  int GetMinBitrate() const;

 private:
  enum UmaState { kNoUpdate, kFirstDone, kDone };

  bool IsInStartPhase(Timestamp now_ms) const;

  void UpdateUmaStatsPacketsLost(Timestamp now_ms, int packets_lost);

  // Updates history of min bitrates.
  // After this method returns min_bitrate_history_.front().second contains the
  // min bitrate used during last kBweIncreaseIntervalMs.
  void UpdateMinHistory(Timestamp now_ms);

  // Cap |bitrate_bps| to [min_bitrate_configured_, max_bitrate_configured_] and
  // set |current_bitrate_bps_| to the capped value and updates the event log.
  void CapBitrateToThresholds(Timestamp now_ms, DataRate bitrate_bps);

  std::deque<std::pair<Timestamp, DataRate> > min_bitrate_history_;

  // incoming filters
  int lost_packets_since_last_loss_update_;
  int expected_packets_since_last_loss_update_;

  DataRate current_bitrate_bps_;
  DataRate min_bitrate_configured_;
  DataRate max_bitrate_configured_;
  Timestamp last_low_bitrate_log_ms_;

  bool has_decreased_since_last_fraction_loss_;
  Timestamp last_loss_feedback_ms_;
  Timestamp last_loss_packet_report_ms_;
  Timestamp last_timeout_ms_;
  uint8_t last_fraction_loss_;
  uint8_t last_logged_fraction_loss_;
  TimeDelta last_round_trip_time_ms_;

  DataRate bwe_incoming_;
  DataRate delay_based_bitrate_bps_;
  Timestamp time_last_decrease_ms_;
  Timestamp first_report_time_ms_;
  int initially_lost_packets_;
  DataRate bitrate_at_2_seconds_kbps_;
  UmaState uma_update_state_;
  UmaState uma_rtt_state_;
  std::vector<bool> rampup_uma_stats_updated_;
  RtcEventLog* event_log_;
  Timestamp last_rtc_event_log_ms_;
  bool in_timeout_experiment_;
  float low_loss_threshold_;
  float high_loss_threshold_;
  DataRate bitrate_threshold_bps_;
};
}  // namespace webrtc
#endif  // MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
