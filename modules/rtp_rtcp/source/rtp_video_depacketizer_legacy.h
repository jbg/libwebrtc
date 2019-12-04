/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_LEGACY_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_LEGACY_H_

#include "absl/types/optional.h"
#include "api/video/video_codec_type.h"
#include "modules/rtp_rtcp/source/rtp_video_depacketizer.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

// Wrapper that rely on RtpDepacketizers to parsing rtp payload.
// TODO(danilchap): Delete this class when all RtpDepacketizers are converted to
// RtoVideoDepacketizers.
class RtpVideoDepacketizerLegacy : public RtpVideoDepacketizer {
 public:
  explicit RtpVideoDepacketizerLegacy(absl::optional<VideoCodecType> codec)
      : codec_(codec) {}
  ~RtpVideoDepacketizerLegacy() override = default;

  absl::optional<Parsed> Parse(rtc::CopyOnWriteBuffer rtp_payload) override;

 private:
  const absl::optional<VideoCodecType> codec_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_DEPACKETIZER_LEGACY_H_
