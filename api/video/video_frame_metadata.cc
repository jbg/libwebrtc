/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame_metadata.h"

#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

VideoFrameMetadata::VideoFrameMetadata(const RTPVideoHeader& header)
    : width_(header.width), height_(header.height) {
  if (header.generic) {
    frame_id_ = absl::make_optional(header.generic->frame_id);
    spatial_index_ = header.generic->spatial_index;
    temporal_index_ = header.generic->temporal_index;
    frame_dependencies_.assign(header.generic->dependencies.begin(),
                               header.generic->dependencies.end());
    decode_target_indications_.assign(
        header.generic->decode_target_indications.begin(),
        header.generic->decode_target_indications.end());
  }
}

}  // namespace webrtc
