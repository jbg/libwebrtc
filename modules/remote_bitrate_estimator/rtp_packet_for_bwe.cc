/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/rtp_packet_for_bwe.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

RtpPacketForBwe RtpPacketForBwe::Create(int64_t arrival_time_ms,
                                        size_t payload_size,
                                        const RTPHeader& header) {
  RTC_DCHECK_GE(arrival_time_ms, 0);
  RTC_DCHECK_LE(arrival_time_ms, std::numeric_limits<int64_t>::max() / 1'000);
  RtpPacketForBwe packet = {
      .arrival_time = Timestamp::Millis(arrival_time_ms),
      .size = DataSize::Bytes(header.headerLength + payload_size),
      .payload_size = DataSize::Bytes(payload_size),
      .ssrc = header.ssrc,
      .timestamp = header.timestamp,
      .feedback_request = header.extension.feedback_request};

  if (header.extension.hasTransmissionTimeOffset) {
    packet.transmission_time_offset = header.extension.transmissionTimeOffset;
  }
  if (header.extension.hasAbsoluteSendTime) {
    packet.absolute_send_time_24bits = header.extension.absoluteSendTime;
  }
  if (header.extension.hasTransportSequenceNumber) {
    packet.transport_sequence_number = header.extension.transportSequenceNumber;
  }

  return packet;
}

RtpPacketForBwe RtpPacketForBwe::Create(const RtpPacketReceived& rtp_packet) {
  RtpPacketForBwe packet = {
      .arrival_time = rtp_packet.arrival_time(),
      .size = DataSize::Bytes(rtp_packet.size()),
      .payload_size = DataSize::Bytes(rtp_packet.payload_size() +
                                      rtp_packet.padding_size()),
      .ssrc = rtp_packet.Ssrc(),
      .timestamp = rtp_packet.Timestamp(),
      .transmission_time_offset = rtp_packet.GetExtension<TransmissionOffset>(),
      .absolute_send_time_24bits = rtp_packet.GetExtension<AbsoluteSendTime>(),
  };

  uint16_t transport_sequence_number = 0;
  if (rtp_packet.GetExtension<TransportSequenceNumberV2>(
          &transport_sequence_number, &packet.feedback_request) ||
      rtp_packet.GetExtension<TransportSequenceNumber>(
          &transport_sequence_number)) {
    packet.transport_sequence_number = transport_sequence_number;
  }

  return packet;
}

}  // namespace webrtc
