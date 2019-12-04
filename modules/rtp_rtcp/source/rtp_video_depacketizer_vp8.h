/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_VP8_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_VP8_H_

#include <cstdint>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_video_depacketizer.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

class RtpVideoDepacketizerVp8 : public RtpVideoDepacketizer {
 public:
  RtpVideoDepacketizerVp8() = default;
  RtpVideoDepacketizerVp8(const RtpVideoDepacketizerVp8&) = delete;
  RtpVideoDepacketizerVp8& operator=(RtpVideoDepacketizerVp8&) = delete;
  ~RtpVideoDepacketizerVp8() override = default;

  // Parses vp8 rtp payload. Returns codec payload offset or zero on error.
  static size_t ParseRtpPayload(rtc::ArrayView<const uint8_t> rtp_payload,
                                RTPVideoHeader* video_header);

  // Implements RtpVideoDepacketizer
  absl::optional<Parsed> Parse(rtc::CopyOnWriteBuffer rtp_payload) override;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_VP8_H_
