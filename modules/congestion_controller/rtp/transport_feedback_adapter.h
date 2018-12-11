/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_TRANSPORT_FEEDBACK_ADAPTER_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_TRANSPORT_FEEDBACK_ADAPTER_H_

#include <deque>
#include <vector>

#include "api/transport/network_types.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/rtp/send_time_history.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class TransportFeedbackAdapter {
 public:
  TransportFeedbackAdapter();
  virtual ~TransportFeedbackAdapter();

  void RegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);
  void DeRegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);

  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info,
                 Timestamp at_time);

  absl::optional<SentPacket> ProcessSentPacket(
      const rtc::SentPacket& sent_packet);

  absl::optional<TransportPacketsFeedback> ProcessTransportFeedback(
      const rtcp::TransportFeedback& feedback,
      Timestamp feedback_time);

  std::vector<PacketFeedback> GetTransportFeedbackVector() const;

  void SetNetworkIds(uint16_t local_id, uint16_t remote_id);

  DataSize GetOutstandingData() const;

 private:
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback);

  std::vector<PacketFeedback> GetPacketFeedbackVector(
      const rtcp::TransportFeedback& feedback,
      Timestamp at_time);

  rtc::SequencedTaskChecker sequenced_checker_;
  SendTimeHistory send_time_history_ RTC_GUARDED_BY(&sequenced_checker_);
  int64_t current_offset_ms_ RTC_GUARDED_BY(&sequenced_checker_);
  int64_t last_timestamp_us_ RTC_GUARDED_BY(&sequenced_checker_);
  std::vector<PacketFeedback> last_packet_feedback_vector_
      RTC_GUARDED_BY(&sequenced_checker_);
  uint16_t local_net_id_ RTC_GUARDED_BY(&sequenced_checker_);
  uint16_t remote_net_id_ RTC_GUARDED_BY(&sequenced_checker_);
  std::vector<PacketFeedbackObserver*> observers_
      RTC_GUARDED_BY(&sequenced_checker_);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_TRANSPORT_FEEDBACK_ADAPTER_H_
