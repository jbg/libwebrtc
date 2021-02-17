/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/svc/scalability_structure_key_svc.h"

#include <bitset>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_bitrate_allocation.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/scalable_video_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// Values to use as LayerFrameConfig::Id
enum : int { kKey, kDelta };
using FramePattern = ScalabilityStructureHelperT3::FramePattern;

DecodeTargetIndication
Dti(int sid, int tid, const ScalableVideoController::LayerFrameConfig& config) {
  if (config.IsKeyframe() || config.Id() == kKey) {
    RTC_DCHECK_EQ(config.TemporalId(), 0);
    return sid < config.SpatialId() ? DecodeTargetIndication::kNotPresent
                                    : DecodeTargetIndication::kSwitch;
  }

  if (sid != config.SpatialId() || tid < config.TemporalId()) {
    return DecodeTargetIndication::kNotPresent;
  }
  if (tid == config.TemporalId() && tid > 0) {
    return DecodeTargetIndication::kDiscardable;
  }
  return DecodeTargetIndication::kSwitch;
}

}  // namespace

ScalabilityStructureKeySvc::ScalabilityStructureKeySvc(int num_spatial_layers,
                                                       int num_temporal_layers)
    : helper_(num_spatial_layers, num_temporal_layers, {}) {
  // There is no point to use this structure without spatial scalability.
  RTC_DCHECK_GT(num_spatial_layers, 1);
}

ScalabilityStructureKeySvc::~ScalabilityStructureKeySvc() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureKeySvc::StreamConfig() const {
  return helper_.StreamConfig();
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureKeySvc::KeyframeConfig() {
  std::vector<LayerFrameConfig> configs;
  configs.reserve(helper_.num_spatial_layers());
  absl::optional<int> spatial_dependency_buffer_id;
  spatial_id_is_enabled_.reset();
  // Disallow temporal references cross T0 on higher temporal layers.
  can_reference_t1_frame_for_spatial_id_.reset();
  for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
    if (!helper_.DecodeTargetIsActive(sid, /*tid=*/0)) {
      continue;
    }
    configs.emplace_back();
    ScalableVideoController::LayerFrameConfig& config = configs.back();
    config.Id(kKey).S(sid).T(0);

    if (spatial_dependency_buffer_id) {
      config.Reference(*spatial_dependency_buffer_id);
    } else {
      config.Keyframe();
    }
    config.Update(helper_.BufferIndex(sid, /*tid=*/0));

    spatial_id_is_enabled_.set(sid);
    spatial_dependency_buffer_id = helper_.BufferIndex(sid, /*tid=*/0);
  }
  return configs;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureKeySvc::T0Config() {
  std::vector<LayerFrameConfig> configs;
  configs.reserve(helper_.num_spatial_layers());
  // Disallow temporal references cross T0 on higher temporal layers.
  can_reference_t1_frame_for_spatial_id_.reset();
  for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
    if (!helper_.DecodeTargetIsActive(sid, /*tid=*/0)) {
      spatial_id_is_enabled_.reset(sid);
      continue;
    }
    configs.emplace_back();
    configs.back().Id(kDelta).S(sid).T(0).ReferenceAndUpdate(
        helper_.BufferIndex(sid, /*tid=*/0));
  }
  return configs;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureKeySvc::T1Config() {
  std::vector<LayerFrameConfig> configs;
  configs.reserve(helper_.num_spatial_layers());
  for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
    if (!helper_.DecodeTargetIsActive(sid, /*tid=*/1)) {
      continue;
    }
    configs.emplace_back();
    ScalableVideoController::LayerFrameConfig& config = configs.back();
    config.Id(kDelta).S(sid).T(1).Reference(
        helper_.BufferIndex(sid, /*tid=*/0));
    if (helper_.num_temporal_layers() > 2) {
      config.Update(helper_.BufferIndex(sid, /*tid=*/1));
    }
  }
  return configs;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureKeySvc::T2Config() {
  std::vector<LayerFrameConfig> configs;
  configs.reserve(helper_.num_spatial_layers());
  for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
    if (!helper_.DecodeTargetIsActive(sid, /*tid=*/2)) {
      continue;
    }
    configs.emplace_back();
    ScalableVideoController::LayerFrameConfig& config = configs.back();
    config.Id(kDelta).S(sid).T(2);
    if (can_reference_t1_frame_for_spatial_id_[sid]) {
      config.Reference(helper_.BufferIndex(sid, /*tid=*/1));
    } else {
      config.Reference(helper_.BufferIndex(sid, /*tid=*/0));
    }
  }
  return configs;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureKeySvc::NextFrameConfig(bool restart) {
  if (!helper_.AnyActiveDecodeTargets()) {
    last_pattern_ = FramePattern::kNone;
    return {};
  }

  if (restart) {
    last_pattern_ = FramePattern::kNone;
  }

  if (last_pattern_ == FramePattern::kNone) {
    RTC_DCHECK_EQ(helper_.NextPattern(last_pattern_), FramePattern::kDeltaT0);
    last_pattern_ = FramePattern::kDeltaT0;
    return KeyframeConfig();
  }
  last_pattern_ = helper_.NextPattern(last_pattern_);
  switch (last_pattern_) {
    case FramePattern::kDeltaT0:
      return T0Config();
    case FramePattern::kDeltaT1:
      return T1Config();
    case FramePattern::kDeltaT2A:
    case FramePattern::kDeltaT2B:
      return T2Config();
    default:
      RTC_NOTREACHED();
      return {};
  }
}

GenericFrameInfo ScalabilityStructureKeySvc::OnEncodeDone(
    const LayerFrameConfig& config) {
  if (config.TemporalId() == 1) {
    can_reference_t1_frame_for_spatial_id_.set(config.SpatialId());
  }

  GenericFrameInfo frame_info = helper_.OnEncodeDone(config);
  for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
    for (int tid = 0; tid < helper_.num_temporal_layers(); ++tid) {
      frame_info.decode_target_indications.push_back(Dti(sid, tid, config));
    }
  }
  if (config.IsKeyframe() || config.Id() == kKey) {
    RTC_DCHECK_EQ(config.TemporalId(), 0);
    for (int sid = config.SpatialId(); sid < helper_.num_spatial_layers();
         ++sid) {
      frame_info.part_of_chain[sid] = true;
    }
  } else if (config.TemporalId() == 0) {
    frame_info.part_of_chain[config.SpatialId()] = true;
  }
  return frame_info;
}

void ScalabilityStructureKeySvc::OnRatesUpdated(
    const VideoBitrateAllocation& bitrates) {
  helper_.SetDecodeTargetsFromAllocation(bitrates);
  for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
    if (!spatial_id_is_enabled_[sid] && helper_.DecodeTargetIsActive(sid, 0)) {
      // Key frame is required to reenable any spatial layer.
      last_pattern_ = FramePattern::kNone;
      break;
    }
  }
}

ScalabilityStructureL2T1Key::~ScalabilityStructureL2T1Key() = default;

FrameDependencyStructure ScalabilityStructureL2T1Key::DependencyStructure()
    const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 1};
  structure.templates.resize(4);
  structure.templates[0].S(0).Dtis("S-").ChainDiffs({2, 1}).FrameDiffs({2});
  structure.templates[1].S(0).Dtis("SS").ChainDiffs({0, 0});
  structure.templates[2].S(1).Dtis("-S").ChainDiffs({1, 2}).FrameDiffs({2});
  structure.templates[3].S(1).Dtis("-S").ChainDiffs({1, 1}).FrameDiffs({1});
  return structure;
}

ScalabilityStructureL2T2Key::~ScalabilityStructureL2T2Key() = default;

FrameDependencyStructure ScalabilityStructureL2T2Key::DependencyStructure()
    const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 4;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 0, 1, 1};
  structure.templates.resize(6);
  auto& templates = structure.templates;
  templates[0].S(0).T(0).Dtis("SSSS").ChainDiffs({0, 0});
  templates[1].S(0).T(0).Dtis("SS--").ChainDiffs({4, 3}).FrameDiffs({4});
  templates[2].S(0).T(1).Dtis("-D--").ChainDiffs({2, 1}).FrameDiffs({2});
  templates[3].S(1).T(0).Dtis("--SS").ChainDiffs({1, 1}).FrameDiffs({1});
  templates[4].S(1).T(0).Dtis("--SS").ChainDiffs({1, 4}).FrameDiffs({4});
  templates[5].S(1).T(1).Dtis("---D").ChainDiffs({3, 2}).FrameDiffs({2});
  return structure;
}

ScalabilityStructureL3T3Key::~ScalabilityStructureL3T3Key() = default;

FrameDependencyStructure ScalabilityStructureL3T3Key::DependencyStructure()
    const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 9;
  structure.num_chains = 3;
  structure.decode_target_protected_by_chain = {0, 0, 0, 1, 1, 1, 2, 2, 2};
  auto& t = structure.templates;
  t.resize(15);
  // Templates are shown in the order frames following them appear in the
  // stream, but in `structure.templates` array templates are sorted by
  // (`spatial_id`, `temporal_id`) since that is a dependency descriptor
  // requirement. Indexes are written in hex for nicer alignment.
  t[0x0].S(0).T(0).Dtis("SSSSSSSSS").ChainDiffs({0, 0, 0});
  t[0x5].S(1).T(0).Dtis("---SSSSSS").ChainDiffs({1, 1, 1}).FrameDiffs({1});
  t[0xA].S(2).T(0).Dtis("------SSS").ChainDiffs({2, 1, 1}).FrameDiffs({1});
  t[0x3].S(0).T(2).Dtis("--D------").ChainDiffs({3, 2, 1}).FrameDiffs({3});
  t[0x8].S(1).T(2).Dtis("-----D---").ChainDiffs({4, 3, 2}).FrameDiffs({3});
  t[0xD].S(2).T(2).Dtis("--------D").ChainDiffs({5, 4, 3}).FrameDiffs({3});
  t[0x2].S(0).T(1).Dtis("-DS------").ChainDiffs({6, 5, 4}).FrameDiffs({6});
  t[0x7].S(1).T(1).Dtis("----DS---").ChainDiffs({7, 6, 5}).FrameDiffs({6});
  t[0xC].S(2).T(1).Dtis("-------DS").ChainDiffs({8, 7, 6}).FrameDiffs({6});
  t[0x4].S(0).T(2).Dtis("--D------").ChainDiffs({9, 8, 7}).FrameDiffs({3});
  t[0x9].S(1).T(2).Dtis("-----D---").ChainDiffs({10, 9, 8}).FrameDiffs({3});
  t[0xE].S(2).T(2).Dtis("--------D").ChainDiffs({11, 10, 9}).FrameDiffs({3});
  t[0x1].S(0).T(0).Dtis("SSS------").ChainDiffs({12, 11, 10}).FrameDiffs({12});
  t[0x6].S(1).T(0).Dtis("---SSS---").ChainDiffs({1, 12, 11}).FrameDiffs({12});
  t[0xB].S(2).T(0).Dtis("------SSS").ChainDiffs({2, 1, 12}).FrameDiffs({12});
  return structure;
}

}  // namespace webrtc
