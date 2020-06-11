/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_LAYERS_ALLOCATION_H_
#define API_VIDEO_VIDEO_LAYERS_ALLOCATION_H_

#include <stdint.h>

#include "api/video/video_codec_constants.h"
#include "api/video/video_content_type.h"

namespace webrtc {

// Describes at which point it is possible to switch between two given spatial
// scalability layers.
enum class SwitchPossibility {
  // Can switch at any key-frame.
  kNeedKeyFrame,
  // Can switch at any key-frame or starting from the frame carrying
  // VideoLayersAllocation (if it isn't a key-frame).
  kOnlyThisFrame,
  // Can switch at this frame and there are regural points where it's possible
  // to switch. No need to request a key-frame immediately.
  // Some codec-specific header should contain additional information for
  // identifying these points.
  kRegularIntervals,
  // Can switch on any frame.
  kAnywhere
};

constexpr int kMaxSpatialLayersOrStreams =
    kMaxSpatialLayers > kMaxSimulcastStreams ? kMaxSpatialLayers
                                             : kMaxSimulcastStreams;

// This struct contains additional stream-level information needed by
// middleboxes to make relay decisions for RTP stream.
// This struct should be populated by encoders and attached to encoded streams.
// From there it is written in RTP header extension, readable for middleboxes.
struct VideoLayersAllocation {
  // Content type: e.g. screenshare vs. camera video.
  VideoContentType content_type;

  // Number of spatial layers for svc and streams for simulcast.
  int num_spatial_layers;

  // Which of the described simulcast streams this stream actually is.
  int spatial_idx;

  // Number of temporal layers per stream or SVC layer.
  int num_temporal_layers[kMaxSpatialLayersOrStreams];

  // Target bitrate for each temporal layer on each spatial layer.
  // Cumulative, i.e. lower temporal layers are included and for SVC all
  // lower spatial layers are included.
  int target_bitrate_kbps[kMaxSpatialLayersOrStreams][kMaxTemporalStreams];

  // True if data below is filled. False means that target framerate,
  // resolutions and switch possibilities data are empty.
  bool has_full_data;

  // Target frame-rate for each temporal layer on each spatial layer.
  uint8_t target_framerate[kMaxSpatialLayersOrStreams][kMaxTemporalStreams];

  // Resolution for each spatial layer/stream.
  uint16_t width[kMaxSpatialLayersOrStreams];
  uint16_t height[kMaxSpatialLayersOrStreams];

  // For each pair of spatial layer/streams indicates when is it possible to
  // switch from one to the other.
  // Diagonal entries are ignorred.
  // Common values:
  // - For simulcast key-frames are allways required for any switch.
  // - For full-svc upswitch is possible on key-frames or when a new layer is
  // enabled, downswitch is always possible.
  // - For k-svc key-frames are required for any switch.
  SwitchPossibility can_switch[kMaxSpatialLayersOrStreams]
                              [kMaxSpatialLayersOrStreams];
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_LAYERS_ALLOCATION_H_
