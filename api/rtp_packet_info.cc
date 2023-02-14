/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_packet_info.h"

#include <algorithm>
#include <utility>

namespace webrtc {

RtpPacketInfo::RtpPacketInfo()
    : ssrc_(0),
      rtp_timestamp_(0),
      receive_time_(Timestamp::MinusInfinity()),
      header_size_(0),
      payload_size_(0) {}

RtpPacketInfo::RtpPacketInfo(uint32_t ssrc,
                             std::vector<uint32_t> csrcs,
                             uint32_t rtp_timestamp,
                             uint16_t rtp_sequence_number,
                             Timestamp receive_time,
                             size_t header_size,
                             size_t payload_size)
    : ssrc_(ssrc),
      csrcs_(std::move(csrcs)),
      rtp_timestamp_(rtp_timestamp),
      rtp_sequence_number_(rtp_sequence_number),
      receive_time_(receive_time),
      header_size_(header_size),
      payload_size_(payload_size) {}

RtpPacketInfo::RtpPacketInfo(const RTPHeader& rtp_header,
                             size_t payload_size,
                             Timestamp receive_time)
    : ssrc_(rtp_header.ssrc),
      rtp_timestamp_(rtp_header.timestamp),
      rtp_sequence_number_(rtp_header.sequenceNumber),
      receive_time_(receive_time),
      header_size_(rtp_header.headerLength + rtp_header.paddingLength),
      payload_size_(payload_size) {
  const auto& extension = rtp_header.extension;
  const auto csrcs_count = std::min<size_t>(rtp_header.numCSRCs, kRtpCsrcSize);

  csrcs_.assign(&rtp_header.arrOfCSRCs[0], &rtp_header.arrOfCSRCs[csrcs_count]);

  if (extension.hasAudioLevel) {
    audio_level_ = extension.audioLevel;
  }

  absolute_capture_time_ = extension.absolute_capture_time;
}

bool operator==(const RtpPacketInfo& lhs, const RtpPacketInfo& rhs) {
  return (lhs.ssrc() == rhs.ssrc()) && (lhs.csrcs() == rhs.csrcs()) &&
         (lhs.rtp_timestamp() == rhs.rtp_timestamp()) &&
         (lhs.receive_time() == rhs.receive_time()) &&
         (lhs.audio_level() == rhs.audio_level()) &&
         (lhs.absolute_capture_time() == rhs.absolute_capture_time()) &&
         (lhs.local_capture_clock_offset() ==
          rhs.local_capture_clock_offset()) &&
         (lhs.rtp_sequence_number() == rhs.rtp_sequence_number()) &&
         (lhs.absolute_send_time() == rhs.absolute_send_time()) &&
         (lhs.header_size() == rhs.header_size()) &&
         (lhs.payload_size() == rhs.payload_size());
}

}  // namespace webrtc
