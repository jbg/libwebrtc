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

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {

// Controls how video should be encoded to be scalable. Outputs results as
// buffer usage configuration for encoder and enough details to communicate the
// scalability structure via dependency descriptor rtp header extension.
class ScalableVideoController {
 public:
  struct StreamConfiguration {
    int num_spatial_layers = 1;
    int num_temporal_layers = 1;
  };
  virtual ~ScalableVideoController() = default;

  // Returns new settings to (re)configure encoder with, or nullopt if previous
  // configuration should be used. When `restart` is true, shouldn't return
  // nullopt and first frame in first call to NextFrameConfig should be
  // a configuration for a key frame.
  virtual absl::optional<StreamConfiguration> EncoderSettings(bool restart) = 0;

  // Returns video structure description in format compatible with
  // dependency descriptor rtp header extension
  virtual FrameDependencyStructure DependencyStructure() const = 0;

  // Returns list of configurations the frame should be encoded with.
  // Returns empty list to indicate frame should be dropped.
  // Normally returns one configuration per active spatial layer.
  virtual std::vector<GenericFrameInfo> NextFrameConfig() = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_SCALABLE_VIDEO_CONTROLLER_H_
