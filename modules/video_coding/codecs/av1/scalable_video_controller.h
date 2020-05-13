/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_SCALABLE_VIDEO_CONTROLLER_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_SCALABLE_VIDEO_CONTROLLER_H_

#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {

// Controls how video should be encoded to be scalable. Outputs results as
// buffer usage configuration for encoder and enough details to communicate the
// scalability structure via dependency descriptor rtp header extension.
class ScalableVideoController {
 public:
  struct StreamConfig {
    int num_spatial_layers = 1;
    int num_temporal_layers = 1;
  };
  struct LayerFrameConfig {
    // Indication frame should be encoded as a key frame. In particular when
    // `is_keyframe=true` property `CodecBufferUsage::referenced` should be
    // ignored and treated as false.
    bool is_keyframe = false;
    // Describes how encoder which buffers encoder allowed to reference and
    // which buffers encoder should update.
    absl::InlinedVector<CodecBufferUsage, kMaxEncoderBuffers> buffers;
    // Contains frame configuration in a format compatible with
    // dependency descriptor rtp header extension.
    // Should be passthrough to EncoderCallback and not used directly.
    GenericFrameInfo rtp_frame_info;
  };
  struct TemporalUnitConfig {
    // When not nullopt, tells encoder to reconfigure it's svc parameters.
    absl::optional<StreamConfig> stream_config;
    // List of configurations the frame should be encoded with.
    // Normally ther is one configuration per active spatial layer.
    // Empty list indicates frame should be dropped.
    std::vector<LayerFrameConfig> layer_frames;
  };

  virtual ~ScalableVideoController() = default;

  // Returns video structure description in format compatible with
  // dependency descriptor rtp header extension.
  virtual FrameDependencyStructure DependencyStructure() const = 0;

  // When `restart` is true, first `LayerFrameConfig` should have `is_keyframe`
  // set to true.
  virtual TemporalUnitConfig NextFrameConfig(bool restart) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_SCALABLE_VIDEO_CONTROLLER_H_
