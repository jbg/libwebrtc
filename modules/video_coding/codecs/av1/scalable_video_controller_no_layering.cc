/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalable_video_controller_no_layering.h"

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"

namespace webrtc {

ScalableVideoControllerNoLayering::~ScalableVideoControllerNoLayering() =
    default;

absl::optional<ScalableVideoController::StreamConfiguration>
ScalableVideoControllerNoLayering::EncoderSettings(bool restart) {
  absl::optional<StreamConfiguration> config;
  if (restart) {
    start_ = true;
  }
  if (start_) {
    config.emplace();
    config->num_spatial_layers = 1;
    config->num_temporal_layers = 1;
  }
  return config;
}

FrameDependencyStructure
ScalableVideoControllerNoLayering::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  FrameDependencyTemplate a_template;
  a_template.decode_target_indications = {DecodeTargetIndication::kSwitch};
  structure.templates.push_back(a_template);
  return structure;
}

std::vector<GenericFrameInfo>
ScalableVideoControllerNoLayering::NextFrameConfig() {
  GenericFrameInfo info;
  info.encoder_buffers = {{/*id=*/0, /*references=*/!start_, /*updates=*/true}};
  info.decode_target_indications = {DecodeTargetIndication::kSwitch};
  start_ = false;
  return {info};
}

}  // namespace webrtc
