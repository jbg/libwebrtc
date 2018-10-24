/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_HDR_METADATA_H_
#define API_VIDEO_HDR_METADATA_H_

namespace webrtc {

struct Chromaticity {
  float x = 0.0f;
  float y = 0.0f;
  bool operator==(const Chromaticity& rhs) const {
    return x == rhs.x && y == rhs.y;
  }
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
// Replicates the HdrMetadata struct defined in
// chromium/src/media/base/hdr_metadata.h
// SMPTE ST 2086 mastering metadata.
struct MasteringMetadata {
  Chromaticity primary_r;
  Chromaticity primary_g;
  Chromaticity primary_b;
  Chromaticity white_point;
  float luminance_max = 0.0f;
  float luminance_min = 0.0f;

  MasteringMetadata();
  MasteringMetadata(const MasteringMetadata& rhs);

  bool operator==(const MasteringMetadata& rhs) const {
    return ((primary_r == rhs.primary_r) && (primary_g == rhs.primary_g) &&
            (primary_b == rhs.primary_b) && (white_point == rhs.white_point) &&
            (luminance_max == rhs.luminance_max) &&
            (luminance_min == rhs.luminance_min));
  }
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct HDRMetadata {
  MasteringMetadata mastering_metadata;
  // Max content light level (CLL), i.e. maximum brightness level present in the
  // stream), in nits.
  unsigned max_content_light_level = 0;
  // Max frame-average light level (FALL), i.e. maximum average brightness of
  // the brightest frame in the stream), in nits.
  unsigned max_frame_average_light_level = 0;

  HDRMetadata();
  HDRMetadata(const HDRMetadata& rhs);

  bool operator==(const HDRMetadata& rhs) const {
    return (
        (max_content_light_level == rhs.max_content_light_level) &&
        (max_frame_average_light_level == rhs.max_frame_average_light_level) &&
        (mastering_metadata == rhs.mastering_metadata));
  }
};

}  // namespace webrtc

#endif  // API_VIDEO_HDR_METADATA_H_
