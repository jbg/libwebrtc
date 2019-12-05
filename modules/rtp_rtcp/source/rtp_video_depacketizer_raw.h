/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_RAW_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_RAW_H_

#include <cstdint>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

class RtpVideoDepacketizerRaw : public VideoRtpDepacketizer {
 public:
  ~RtpVideoDepacketizerRaw() override = default;

  absl::optional<ParsedRtpPayload> Parse(
      rtc::CopyOnWriteBuffer rtp_payload) override;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_RAW_H_
