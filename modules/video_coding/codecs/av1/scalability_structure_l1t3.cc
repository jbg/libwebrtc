/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_l1t3.h"

#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr auto kNotPresent = DecodeTargetIndication::kNotPresent;
constexpr auto kDiscardable = DecodeTargetIndication::kDiscardable;
constexpr auto kSwitch = DecodeTargetIndication::kSwitch;

constexpr DecodeTargetIndication kDtis[3][3] = {
    {kSwitch, kSwitch, kSwitch},               // T0
    {kNotPresent, kDiscardable, kSwitch},      // T1
    {kNotPresent, kNotPresent, kDiscardable},  // T2
};

}  // namespace

ScalabilityStructureL1T3::~ScalabilityStructureL1T3() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureL1T3::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 1;
  result.num_temporal_layers = 3;
  return result;
}

FrameDependencyStructure ScalabilityStructureL1T3::DependencyStructure() const {
  using Builder = GenericFrameInfo::Builder;
  FrameDependencyStructure structure;
  structure.num_decode_targets = 3;
  structure.num_chains = 1;
  structure.decode_target_protected_by_chain = {0, 0, 0};
  structure.templates = {
      Builder().T(0).Dtis("SSS").ChainDiffs({0}).Build(),
      Builder().T(0).Dtis("SSS").ChainDiffs({4}).Fdiffs({4}).Build(),
      Builder().T(1).Dtis("-DS").ChainDiffs({2}).Fdiffs({2}).Build(),
      Builder().T(2).Dtis("--D").ChainDiffs({1}).Fdiffs({1}).Build(),
      Builder().T(2).Dtis("--D").ChainDiffs({3}).Fdiffs({1}).Build(),
  };
  return structure;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureL1T3::NextFrameConfig(bool restart) {
  if (restart) {
    next_pattern_ = kKeyFrame;
  }
  std::vector<LayerFrameConfig> result(1);

  switch (next_pattern_) {
    case kKeyFrame:
      result[0].is_keyframe = true;
      result[0].temporal_id = 0;
      result[0].buffers = {{/*id=*/0, /*references=*/false, /*updates=*/true}};
      next_pattern_ = kDeltaFrameT2A;
      break;
    case kDeltaFrameT2A:
      result[0].temporal_id = 2;
      result[0].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/false}};
      next_pattern_ = kDeltaFrameT1;
      break;
    case kDeltaFrameT1:
      result[0].temporal_id = 1;
      result[0].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/false},
                           {/*id=*/1, /*references=*/false, /*updates=*/true}};
      next_pattern_ = kDeltaFrameT2B;
      break;
    case kDeltaFrameT2B:
      result[0].temporal_id = 2;
      result[0].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/false},
                           {/*id=*/1, /*references=*/true, /*updates=*/false}};
      next_pattern_ = kDeltaFrameT0;
      break;
    case kDeltaFrameT0:
      result[0].temporal_id = 0;
      result[0].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/true}};
      next_pattern_ = kDeltaFrameT2A;
      break;
  }
  return result;
}

absl::optional<GenericFrameInfo> ScalabilityStructureL1T3::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info;
  if (config.temporal_id < 0 ||
      config.temporal_id >= int{ABSL_ARRAYSIZE(kDtis)}) {
    RTC_LOG(LS_ERROR) << "Unexpected temporal id " << config.temporal_id;
    return frame_info;
  }
  frame_info.emplace();
  frame_info->temporal_id = config.temporal_id;
  frame_info->encoder_buffers = std::move(config.buffers);
  frame_info->decode_target_indications.assign(
      std::begin(kDtis[config.temporal_id]),
      std::end(kDtis[config.temporal_id]));
  frame_info->part_of_chain = {config.temporal_id == 0};
  return frame_info;
}

}  // namespace webrtc
