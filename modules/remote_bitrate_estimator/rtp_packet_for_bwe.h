/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_RTP_PACKET_FOR_BWE_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_RTP_PACKET_FOR_BWE_H_

#include <cstddef>
#include <cstdint>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

struct RtpPacketForBwe {
  // Use factory functions instead of constructors to keep ability to create
  // `RtpPacketForBwe` struct with designated initializers.
  static RtpPacketForBwe Create(int64_t arrival_time_ms,
                                size_t payload_size,
                                const RTPHeader& header);
  static RtpPacketForBwe Create(const RtpPacketReceived& rtp_packet);

  const Timestamp arrival_time;
  // Size of the packet including rtp header.
  const DataSize size = DataSize::Zero();
  // Size of the packet excluding rtp header.
  const DataSize payload_size = DataSize::Zero();
  const uint32_t ssrc;
  const uint32_t timestamp = 0;

  absl::optional<int32_t> transmission_time_offset;
  absl::optional<uint32_t> absolute_send_time_24bits;
  absl::optional<uint16_t> transport_sequence_number;
  absl::optional<FeedbackRequest> feedback_request;
};

}  // namespace webrtc

#endif  //  MODULES_REMOTE_BITRATE_ESTIMATOR_RTP_PACKET_FOR_BWE_H_
