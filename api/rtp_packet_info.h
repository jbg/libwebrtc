/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_PACKET_INFO_H_
#define API_RTP_PACKET_INFO_H_

#include <cstdint>
#include <vector>

#include "absl/types/optional.h"
#include "api/ref_counted_base.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"

namespace webrtc {

// Immutable structure to hold information about a received |RtpPacket|.
class RtpPacketInfo final : public rtc::RefCountedBase {
 public:
  static rtc::scoped_refptr<RtpPacketInfo> Create(
      uint32_t ssrc,
      std::vector<uint32_t> csrcs,
      uint16_t sequence_number,
      uint32_t rtp_timestamp,
      absl::optional<uint8_t> audio_level,
      int64_t receive_time_ms);

  static rtc::scoped_refptr<RtpPacketInfo> Create(const RTPHeader& rtp_header,
                                                  int64_t receive_time_ms);

  uint32_t ssrc() const { return ssrc_; }
  const std::vector<uint32_t>& csrcs() const { return csrcs_; }
  uint16_t sequence_number() const { return sequence_number_; }
  uint32_t rtp_timestamp() const { return rtp_timestamp_; }

  absl::optional<uint8_t> audio_level() const { return audio_level_; }

  int64_t receive_time_ms() const { return receive_time_ms_; }

 private:
  RtpPacketInfo(uint32_t ssrc,
                std::vector<uint32_t> csrcs,
                uint16_t sequence_number,
                uint32_t rtp_timestamp,
                absl::optional<uint8_t> audio_level,
                int64_t receive_time_ms);

  ~RtpPacketInfo() override {}

  // Fields from the RTP header:
  //   https://tools.ietf.org/html/rfc3550#section-5.1
  const uint32_t ssrc_;
  const std::vector<uint32_t> csrcs_;
  const uint16_t sequence_number_;
  const uint32_t rtp_timestamp_;

  // Fields from the Audio Level header extension:
  //   https://tools.ietf.org/html/rfc6464#section-3
  const absl::optional<uint8_t> audio_level_;

  // Local |Clock|-based timestamp of when the packet was received.
  const int64_t receive_time_ms_;
};

}  // namespace webrtc

#endif  // API_RTP_PACKET_INFO_H_
