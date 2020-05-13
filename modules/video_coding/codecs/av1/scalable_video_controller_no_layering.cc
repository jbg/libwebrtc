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

#include <utility>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"

namespace webrtc {

ScalableVideoControllerNoLayering::~ScalableVideoControllerNoLayering() =
    default;

FrameDependencyStructure
ScalableVideoControllerNoLayering::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  FrameDependencyTemplate a_template;
  a_template.decode_target_indications = {DecodeTargetIndication::kSwitch};
  structure.templates.push_back(a_template);
  return structure;
}

ScalableVideoController::TemporalUnitConfig
ScalableVideoControllerNoLayering::NextFrameConfig(bool restart) {
  if (restart) {
    start_ = true;
  }
  TemporalUnitConfig result;
  if (start_) {
    StreamConfig& config = result.stream_config.emplace();
    config.num_spatial_layers = 1;
    config.num_temporal_layers = 1;
  }
  LayerFrameConfig layer_frame;
  layer_frame.is_keyframe = start_;
  layer_frame.buffers = {{/*id=*/0, /*references=*/!start_, /*updates=*/true}};
  layer_frame.rtp_frame_info.encoder_buffers = layer_frame.buffers;
  layer_frame.rtp_frame_info.decode_target_indications = {
      DecodeTargetIndication::kSwitch};
  result.layer_frames.push_back(std::move(layer_frame));

  start_ = false;
  return result;
}

}  // namespace webrtc
