/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtp_video_depacketizer_create.h"

#include <memory>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/rtp_video_depacketizer.h"
#include "modules/rtp_rtcp/source/rtp_video_depacketizer_legacy.h"
#include "modules/rtp_rtcp/source/rtp_video_depacketizer_vp8.h"

namespace webrtc {

std::unique_ptr<RtpVideoDepacketizer> RtpVideoDepacketizerCreate(
    VideoCodecType codec) {
  switch (codec) {
    case kVideoCodecVP8:
      return std::make_unique<RtpVideoDepacketizerVp8>();
    default:
      return std::make_unique<RtpVideoDepacketizerLegacy>(codec);
  }
}

}  // namespace webrtc
