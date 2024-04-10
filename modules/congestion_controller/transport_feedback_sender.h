#ifndef MODULES_CONGESTION_CONTROLLER_TRANSPORT_FEEDBACK_SENDER_H_
#define MODULES_CONGESTION_CONTROLLER_TRANSPORT_FEEDBACK_SENDER_H_
/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "api/sequence_checker.h"
#include "api/units/data_rate.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/network/ecn_marking.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
namespace webrtc {

// Class responsible for sending transport feedback following the RFC-8888
// specification.
class TransportFeedbackSender {
 public:
  using RtcpSender = std::function<void(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets)>;
  explicit TransportFeedbackSender(Clock& clock, RtcpSender rtcp_sender);

  void OnReceivedPacket(const RtpPacketReceived& packet);

  // Sends periodic feedback if it is time to send it.
  // Returns time until next call to Process should be made.
  TimeDelta Process(Timestamp now);

  void OnTargetBitrateChanged(DataRate bitrate);
  void SetTransportOverhead(DataSize overhead_per_packet);

 private:
  void MaybeSendFeedback() RTC_RUN_ON(sequence_checker_);

  struct PacketInfo {
    uint16_t sequence_number = 0;
    int64_t unwrapped_sequence_number = 0;

    //  Time offset from  compact_ntp_timestamp.
    Timestamp arrival_time;
    // = TimeDelta::Zero();
    rtc::EcnMarking ecn = rtc::EcnMarking::kNotEct;
  };

  std::map</*ssrc=*/uint32_t, std::vector<PacketInfo>> packet_map_;
  std::map</*ssrc=*/uint32_t, SeqNumUnwrapper<uint16_t>>
      sequence_number_unwrappers_;

  webrtc::SequenceChecker sequence_checker_;
  Clock& clock_;
  const RtcpSender rtcp_sender_;
  Timestamp last_process_time_ = Timestamp::MinusInfinity();
  TimeDelta send_interval_;
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_TRANSPORT_FEEDBACK_SENDER_H_
