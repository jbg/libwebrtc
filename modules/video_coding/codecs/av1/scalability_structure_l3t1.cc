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
    {kSwitch, kSwitch, kSwitch},            // Key, S0
    {kNotPresent, kSwitch, kSwitch},        // Key, S1
    {kNotPresent, kNotPresent, kSwitch},    // Key, S2
    {kSwitch, kRequired, kRequired},        // Delta, S0
    {kNotPresent, kRequired, kRequired},    // Delta, S1
    {kNotPresent, kNotPresent, kRequired},  // Delta, S2
};

}  // namespace

ScalabilityStructureL3T1::~ScalabilityStructureL3T1() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureL3T1::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 3;
  result.num_temporal_layers = 1;
  return result;
}

FrameDependencyStructure ScalabilityStructureL3T1::DependencyStructure() const {
  using Builder = GenericFrameInfo::Builder;
  FrameDependencyStructure structure;
  structure.num_decode_targets = 3;
  structure.num_chains = 3;
  structure.decode_target_protected_by_chain = {0, 1, 2};
  structure.templates = {
      Builder().S(0).Dtis("SRR").Fdiffs({3}).ChainDiffs({3, 2, 1}).Build(),
      Builder().S(0).Dtis("SSS").ChainDiffs({0, 0, 0}).Build(),
      Builder().S(1).Dtis("-RR").Fdiffs({3, 1}).ChainDiffs({1, 1, 1}).Build(),
      Builder().S(1).Dtis("-SS").Fdiffs({1}).ChainDiffs({1, 1, 1}).Build(),
      Builder().S(2).Dtis("--R").Fdiffs({3, 1}).ChainDiffs({2, 1, 1}).Build(),
      Builder().S(2).Dtis("--S").Fdiffs({1}).ChainDiffs({2, 1, 1}).Build(),
  };
  return structure;
}

ScalableVideoController::LayerFrameConfig
ScalabilityStructureL3T1::KeyFrameConfig() const {
  LayerFrameConfig result;
  result.id = 0;
  result.is_keyframe = true;
  result.spatial_id = 0;
  result.buffers = {{/*id=*/0, /*references=*/false, /*updates=*/true}};
  return result;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureL3T1::NextFrameConfig(bool restart) {
  std::vector<LayerFrameConfig> result(3);

  // Buffer i keeps latest frame for spatial layer i
  if (restart || keyframe_) {
    result[0] = KeyFrameConfig();

    result[1].id = 1;
    result[1].spatial_id = 1;
    result[1].buffers = {{/*id=*/1, /*referenced=*/false, /*updated=*/true},
                         {/*id=*/0, /*referenced=*/true, /*updated=*/false}};

    result[2].id = 2;
    result[2].spatial_id = 2;
    result[2].buffers = {{/*id=*/2, /*referenced=*/false, /*updated=*/true},
                         {/*id=*/1, /*referenced=*/true, /*updated=*/false}};
    keyframe_ = false;
  } else {
    result[0].id = 3;
    result[0].spatial_id = 0;
    result[0].buffers = {{/*id=*/0, /*referenced=*/true, /*updated=*/true}};

    result[1].id = 4;
    result[1].spatial_id = 1;
    result[1].buffers = {{/*id=*/1, /*referenced=*/true, /*updated=*/true},
                         {/*id=*/0, /*referenced=*/true, /*updated=*/false}};

    result[2].id = 5;
    result[2].spatial_id = 2;
    result[2].buffers = {{/*id=*/2, /*referenced=*/true, /*updated=*/true},
                         {/*id=*/1, /*referenced=*/true, /*updated=*/false}};
  }
  return result;
}

absl::optional<GenericFrameInfo> ScalabilityStructureL3T1::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info;
  if (config.is_keyframe) {
    config = KeyFrameConfig();
  }

  if (config.id < 0 || config.id >= int{ABSL_ARRAYSIZE(kDtis)}) {
    RTC_LOG(LS_ERROR) << "Unexpected config id " << config.id;
    return frame_info;
  }
  frame_info.emplace();
  frame_info->spatial_id = config.spatial_id;
  frame_info->temporal_id = config.temporal_id;
  frame_info->encoder_buffers = std::move(config.buffers);
  frame_info->decode_target_indications.assign(std::begin(kDtis[config.id]),
                                               std::end(kDtis[config.id]));
  frame_info->part_of_chain = {config.spatial_id == 0, config.spatial_id <= 1,
                               true};
  return frame_info;
}

}  // namespace webrtc
