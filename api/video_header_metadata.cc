/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_header_metadata.h"

namespace webrtc {

VideoHeaderMetadata::VideoHeaderMetadata(const RTPVideoHeader& header)
    : width_(header.width),
      height_(header.height),
      frame_id_(header.generic
                    ? absl::optional<uint64_t>(header.generic->frame_id)
                    : absl::nullopt),
      spatial_index_(header.generic ? header.generic->spatial_index : 0),
      temporal_index_(header.generic ? header.generic->temporal_index : 0),
      frame_dependencies_(
          header.generic
              ? std::vector<int64_t>(header.generic->dependencies.begin(),
                                     header.generic->dependencies.end())
              : std::vector<int64_t>()),
      decode_target_indications_(
          header.generic
              ? std::vector<DecodeTargetIndication>(
                    header.generic->decode_target_indications.begin(),
                    header.generic->decode_target_indications.end())
              : std::vector<DecodeTargetIndication>()) {}

}  // namespace webrtc
