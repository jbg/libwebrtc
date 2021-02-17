/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/svc/scalability_structure_full_svc.h"

#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
enum : int { kKey, kDelta };
using FramePattern = ScalabilityStructureHelperT3::FramePattern;

DecodeTargetIndication
Dti(int sid, int tid, const ScalableVideoController::LayerFrameConfig& config) {
  if (sid < config.SpatialId() || tid < config.TemporalId()) {
    return DecodeTargetIndication::kNotPresent;
  }
  if (sid == config.SpatialId()) {
    if (tid == 0) {
      RTC_DCHECK_EQ(config.TemporalId(), 0);
      return DecodeTargetIndication::kSwitch;
    }
    if (tid == config.TemporalId()) {
      return DecodeTargetIndication::kDiscardable;
    }
    if (tid > config.TemporalId()) {
      RTC_DCHECK_GT(tid, config.TemporalId());
      return DecodeTargetIndication::kSwitch;
    }
  }
  RTC_DCHECK_GT(sid, config.SpatialId());
  RTC_DCHECK_GE(tid, config.TemporalId());
  if (config.IsKeyframe() || config.Id() == kKey) {
    return DecodeTargetIndication::kSwitch;
  }
  return DecodeTargetIndication::kRequired;
}
}  // namespace

ScalabilityStructureFullSvc::ScalabilityStructureFullSvc(
    int num_spatial_layers,
    int num_temporal_layers,
    ScalingFactor resolution_factor)
    : helper_(num_spatial_layers, num_temporal_layers, resolution_factor) {}

ScalabilityStructureFullSvc::~ScalabilityStructureFullSvc() = default;

ScalabilityStructureFullSvc::StreamLayersConfig
ScalabilityStructureFullSvc::StreamConfig() const {
  return helper_.StreamConfig();
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureFullSvc::NextFrameConfig(bool restart) {
  std::vector<LayerFrameConfig> configs;
  if (!helper_.AnyActiveDecodeTargets()) {
    last_pattern_ = FramePattern::kNone;
    return configs;
  }
  configs.reserve(helper_.num_spatial_layers());

  if (last_pattern_ == FramePattern::kNone || restart) {
    can_reference_t0_frame_for_spatial_id_.reset();
    last_pattern_ = FramePattern::kNone;
  }
  FramePattern current_pattern = helper_.NextPattern(last_pattern_);

  absl::optional<int> spatial_dependency_buffer_id;
  switch (current_pattern) {
    case FramePattern::kDeltaT0:
      // Disallow temporal references cross T0 on higher temporal layers.
      can_reference_t1_frame_for_spatial_id_.reset();
      for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
        if (!helper_.DecodeTargetIsActive(sid, /*tid=*/0)) {
          // Next frame from the spatial layer `sid` shouldn't depend on
          // potentially old previous frame from the spatial layer `sid`.
          can_reference_t0_frame_for_spatial_id_.reset(sid);
          continue;
        }
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(last_pattern_ == FramePattern::kNone ? kKey : kDelta)
            .S(sid)
            .T(0);

        if (spatial_dependency_buffer_id) {
          config.Reference(*spatial_dependency_buffer_id);
        } else if (last_pattern_ == FramePattern::kNone) {
          config.Keyframe();
        }

        if (can_reference_t0_frame_for_spatial_id_[sid]) {
          config.ReferenceAndUpdate(helper_.BufferIndex(sid, /*tid=*/0));
        } else {
          // TODO(bugs.webrtc.org/11999): Propagate chain restart on delta frame
          // to ChainDiffCalculator
          config.Update(helper_.BufferIndex(sid, /*tid=*/0));
        }

        can_reference_t0_frame_for_spatial_id_.set(sid);
        spatial_dependency_buffer_id = helper_.BufferIndex(sid, /*tid=*/0);
      }
      break;
    case FramePattern::kDeltaT1:
      for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
        if (!helper_.DecodeTargetIsActive(sid, /*tid=*/1) ||
            !can_reference_t0_frame_for_spatial_id_[sid]) {
          continue;
        }
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(kDelta).S(sid).T(1);
        // Temporal reference.
        config.Reference(helper_.BufferIndex(sid, /*tid=*/0));
        // Spatial reference unless this is the lowest active spatial layer.
        if (spatial_dependency_buffer_id) {
          config.Reference(*spatial_dependency_buffer_id);
        }
        // No frame reference top layer frame, so no need save it into a buffer.
        if (helper_.num_temporal_layers() > 2 ||
            sid < helper_.num_spatial_layers() - 1) {
          config.Update(helper_.BufferIndex(sid, /*tid=*/1));
        }
        spatial_dependency_buffer_id = helper_.BufferIndex(sid, /*tid=*/1);
      }
      break;
    case FramePattern::kDeltaT2A:
    case FramePattern::kDeltaT2B:
      for (int sid = 0; sid < helper_.num_spatial_layers(); ++sid) {
        if (!helper_.DecodeTargetIsActive(sid, /*tid=*/2) ||
            !can_reference_t0_frame_for_spatial_id_[sid]) {
          continue;
        }
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(kDelta).S(sid).T(2);
        // Temporal reference.
        if (current_pattern == FramePattern::kDeltaT2B &&
            can_reference_t1_frame_for_spatial_id_[sid]) {
          config.Reference(helper_.BufferIndex(sid, /*tid=*/1));
        } else {
          config.Reference(helper_.BufferIndex(sid, /*tid=*/0));
        }
        // Spatial reference unless this is the lowest active spatial layer.
        if (spatial_dependency_buffer_id) {
          config.Reference(*spatial_dependency_buffer_id);
        }
        // No frame reference top layer frame, so no need save it into a buffer.
        if (sid < helper_.num_spatial_layers() - 1) {
          config.Update(helper_.BufferIndex(sid, /*tid=*/2));
        }
        spatial_dependency_buffer_id = helper_.BufferIndex(sid, /*tid=*/2);
      }
      break;
    case FramePattern::kNone:
      RTC_NOTREACHED();
      break;
  }

  if (configs.empty() && !restart) {
    RTC_LOG(LS_WARNING)
        << "Failed to generate configuration for L"
        << helper_.num_spatial_layers() << "T" << helper_.num_temporal_layers()
        << " with active decode targets " << helper_.PrintableDecodeTargets()
        << " and transition from "
        << ScalabilityStructureHelperT3::kFramePatternNames[last_pattern_]
        << " to "
        << ScalabilityStructureHelperT3::kFramePatternNames[current_pattern]
        << ". Resetting.";
    return NextFrameConfig(/*restart=*/true);
  }

  last_pattern_ = current_pattern;
  return configs;
}

GenericFrameInfo ScalabilityStructureFullSvc::OnEncodeDone(
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
  if (config.TemporalId() == 0) {
    for (int sid = config.SpatialId(); sid < helper_.num_spatial_layers();
         ++sid) {
      frame_info.part_of_chain[sid] = true;
    }
  }
  return frame_info;
}

void ScalabilityStructureFullSvc::OnRatesUpdated(
    const VideoBitrateAllocation& bitrates) {
  helper_.SetDecodeTargetsFromAllocation(bitrates);
}

FrameDependencyStructure ScalabilityStructureL1T2::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 1;
  structure.decode_target_protected_by_chain = {0, 0};
  structure.templates.resize(3);
  structure.templates[0].T(0).Dtis("SS").ChainDiffs({0});
  structure.templates[1].T(0).Dtis("SS").ChainDiffs({2}).FrameDiffs({2});
  structure.templates[2].T(1).Dtis("-D").ChainDiffs({1}).FrameDiffs({1});
  return structure;
}

FrameDependencyStructure ScalabilityStructureL1T3::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 3;
  structure.num_chains = 1;
  structure.decode_target_protected_by_chain = {0, 0, 0};
  structure.templates.resize(5);
  structure.templates[0].T(0).Dtis("SSS").ChainDiffs({0});
  structure.templates[1].T(0).Dtis("SSS").ChainDiffs({4}).FrameDiffs({4});
  structure.templates[2].T(1).Dtis("-DS").ChainDiffs({2}).FrameDiffs({2});
  structure.templates[3].T(2).Dtis("--D").ChainDiffs({1}).FrameDiffs({1});
  structure.templates[4].T(2).Dtis("--D").ChainDiffs({3}).FrameDiffs({1});
  return structure;
}

FrameDependencyStructure ScalabilityStructureL2T1::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 1};
  structure.templates.resize(4);
  structure.templates[0].S(0).Dtis("SR").ChainDiffs({2, 1}).FrameDiffs({2});
  structure.templates[1].S(0).Dtis("SS").ChainDiffs({0, 0});
  structure.templates[2].S(1).Dtis("-S").ChainDiffs({1, 1}).FrameDiffs({2, 1});
  structure.templates[3].S(1).Dtis("-S").ChainDiffs({1, 1}).FrameDiffs({1});
  return structure;
}

FrameDependencyStructure ScalabilityStructureL2T2::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 4;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 0, 1, 1};
  structure.templates.resize(6);
  auto& templates = structure.templates;
  templates[0].S(0).T(0).Dtis("SSSS").ChainDiffs({0, 0});
  templates[1].S(0).T(0).Dtis("SSRR").ChainDiffs({4, 3}).FrameDiffs({4});
  templates[2].S(0).T(1).Dtis("-D-R").ChainDiffs({2, 1}).FrameDiffs({2});
  templates[3].S(1).T(0).Dtis("--SS").ChainDiffs({1, 1}).FrameDiffs({1});
  templates[4].S(1).T(0).Dtis("--SS").ChainDiffs({1, 1}).FrameDiffs({4, 1});
  templates[5].S(1).T(1).Dtis("---D").ChainDiffs({3, 2}).FrameDiffs({2, 1});
  return structure;
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

FrameDependencyStructure ScalabilityStructureL3T3::DependencyStructure() const {
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
  t[0x1].S(0).T(0).Dtis("SSSSSSSSS").ChainDiffs({0, 0, 0});
  t[0x6].S(1).T(0).Dtis("---SSSSSS").ChainDiffs({1, 1, 1}).FrameDiffs({1});
  t[0xB].S(2).T(0).Dtis("------SSS").ChainDiffs({2, 1, 1}).FrameDiffs({1});
  t[0x3].S(0).T(2).Dtis("--D--R--R").ChainDiffs({3, 2, 1}).FrameDiffs({3});
  t[0x8].S(1).T(2).Dtis("-----D--R").ChainDiffs({4, 3, 2}).FrameDiffs({3, 1});
  t[0xD].S(2).T(2).Dtis("--------D").ChainDiffs({5, 4, 3}).FrameDiffs({3, 1});
  t[0x2].S(0).T(1).Dtis("-DS-RR-RR").ChainDiffs({6, 5, 4}).FrameDiffs({6});
  t[0x7].S(1).T(1).Dtis("----DS-RR").ChainDiffs({7, 6, 5}).FrameDiffs({6, 1});
  t[0xC].S(2).T(1).Dtis("-------DS").ChainDiffs({8, 7, 6}).FrameDiffs({6, 1});
  t[0x4].S(0).T(2).Dtis("--D--R--R").ChainDiffs({9, 8, 7}).FrameDiffs({3});
  t[0x9].S(1).T(2).Dtis("-----D--R").ChainDiffs({10, 9, 8}).FrameDiffs({3, 1});
  t[0xE].S(2).T(2).Dtis("--------D").ChainDiffs({11, 10, 9}).FrameDiffs({3, 1});
  t[0x0].S(0).T(0).Dtis("SSSRRRRRR").ChainDiffs({12, 11, 10}).FrameDiffs({12});
  t[0x5].S(1).T(0).Dtis("---SSSRRR").ChainDiffs({1, 1, 1}).FrameDiffs({12, 1});
  t[0xA].S(2).T(0).Dtis("------SSS").ChainDiffs({2, 1, 1}).FrameDiffs({12, 1});
  return structure;
}

}  // namespace webrtc
