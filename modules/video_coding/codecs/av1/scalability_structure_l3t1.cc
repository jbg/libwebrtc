/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_l3t1.h"

#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr auto kNotPresent = DecodeTargetIndication::kNotPresent;
constexpr auto kSwitch = DecodeTargetIndication::kSwitch;
constexpr auto kRequired = DecodeTargetIndication::kRequired;

constexpr DecodeTargetIndication kDtis[6][3] = {
    {kSwitch, kSwitch, kSwitch},          // Key, S0
    {kNotPresent, kSwitch, kSwitch},      // Key, S1
    {kNotPresent, kNotPresent, kSwitch},  // Key, S2
    {kSwitch, kRequired, kRequired},      // Delta, S0
    {kNotPresent, kSwitch, kRequired},    // Delta, S1
    {kNotPresent, kNotPresent, kSwitch},  // Delta, S2
};

}  // namespace

ScalabilityStructureL3T1::~ScalabilityStructureL3T1() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureL3T1::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 3;
  result.num_temporal_layers = 1;
  result.scaling_factor_num[0] = 1;
  result.scaling_factor_den[0] = 4;
  result.scaling_factor_num[1] = 1;
  result.scaling_factor_den[1] = 2;
  return result;
}

FrameDependencyStructure ScalabilityStructureL3T1::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 3;
  structure.num_chains = 3;
  structure.decode_target_protected_by_chain = {0, 1, 2};
  auto& templates = structure.templates;
  templates.resize(6);
  templates[0].S(0).Dtis("SRR").ChainDiffs({3, 2, 1}).FrameDiffs({3});
  templates[1].S(0).Dtis("SSS").ChainDiffs({0, 0, 0});
  templates[2].S(1).Dtis("-SR").ChainDiffs({1, 1, 1}).FrameDiffs({3, 1});
  templates[3].S(1).Dtis("-SS").ChainDiffs({1, 1, 1}).FrameDiffs({1});
  templates[4].S(2).Dtis("--S").ChainDiffs({2, 1, 1}).FrameDiffs({3, 1});
  templates[5].S(2).Dtis("--S").ChainDiffs({2, 1, 1}).FrameDiffs({1});
  return structure;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureL3T1::NextFrameConfig(bool restart) {
  std::vector<LayerFrameConfig> configs;
  configs.reserve(3);

  // Buffer i keeps latest frame for spatial layer i
  if (keyframe_ || restart) {
    for (int sid = 0; sid < 3; ++sid) {
      use_temporal_dependency_[sid] = false;
    }
    restart = true;
  }

  absl::optional<int> spatial_dependency_buffer_id;
  for (int sid = 0; sid < 3; ++sid) {
    if (!active_decode_targets_[sid]) {
      // Next frame from the spatial layer `sid` shouldn't depend on potentially
      // very old previous frame from the spatial layer `sid`.
      use_temporal_dependency_[sid] = false;
      continue;
    }
    configs.emplace_back();
    ScalableVideoController::LayerFrameConfig& config = configs.back().S(sid);
    config.Id(restart ? 1 : 0);

    if (spatial_dependency_buffer_id) {
      config.Reference(*spatial_dependency_buffer_id);
    } else if (restart) {
      config.Keyframe();
    }

    if (use_temporal_dependency_[sid]) {
      config.ReferenceAndUpdate(sid);
    } else {
      // TODO(danilchap): Propagate chain restart on delta frame to
      // ChainDiffCalculator
      config.Update(sid);
    }
    spatial_dependency_buffer_id = sid;
    use_temporal_dependency_[sid] = true;
  }
  keyframe_ = false;
  return configs;
}

absl::optional<GenericFrameInfo> ScalabilityStructureL3T1::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info;
  int dti_index =
      ((config.IsKeyframe() || config.Id() == 1) ? 0 : 3) + config.SpatialId();

  if (dti_index < 0 || dti_index >= int{ABSL_ARRAYSIZE(kDtis)}) {
    RTC_LOG(LS_ERROR) << "Unexpected layer frame config id " << config.Id()
                      << ", spatial id: " << config.SpatialId();
    return frame_info;
  }
  frame_info.emplace();
  frame_info->spatial_id = config.SpatialId();
  frame_info->temporal_id = config.TemporalId();
  frame_info->encoder_buffers = config.Buffers();
  frame_info->decode_target_indications.assign(std::begin(kDtis[dti_index]),
                                               std::end(kDtis[dti_index]));
  frame_info->part_of_chain = {config.SpatialId() == 0, config.SpatialId() <= 1,
                               true};
  frame_info->active_decode_targets = active_decode_targets_;
  return frame_info;
}

void ScalabilityStructureL3T1::OnRatesUpdated(
    const VideoBitrateAllocation& bitrates) {
  for (int sid = 0; sid < 3; ++sid) {
    active_decode_targets_.set(sid, bitrates.GetBitrate(sid, 0) > 0);
  }
}

}  // namespace webrtc
