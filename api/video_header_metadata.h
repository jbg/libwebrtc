/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_HEADER_METADATA_H_
#define API_VIDEO_HEADER_METADATA_H_

#include <vector>

#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

// A subset of metadata from the RTP video header, exposed in insertable streams
// API.
class VideoHeaderMetadata {
 public:
  explicit VideoHeaderMetadata(const RTPVideoHeader& header);

  uint16_t GetWidth() const { return width_; }
  uint16_t GetHeight() const { return height_; }
  absl::optional<int64_t> GetFrameId() const { return frame_id_; }
  int GetSpatialIndex() const { return spatial_index_; }
  int GetTemporalIndex() const { return temporal_index_; }

  std::vector<int64_t> GetFrameDependencies() const {
    return frame_dependencies_;
  }

  std::vector<DecodeTargetIndication> GetDecodeTargetIndications() const {
    return decode_target_indications_;
  }

 private:
  const int16_t width_;
  const int16_t height_;
  const absl::optional<int64_t> frame_id_;
  const int spatial_index_;
  const int temporal_index_;
  const std::vector<int64_t> frame_dependencies_;
  const std::vector<DecodeTargetIndication> decode_target_indications_;
};
}  // namespace webrtc

#endif  // API_VIDEO_HEADER_METADATA_H_
