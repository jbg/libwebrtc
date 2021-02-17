/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/svc/scalability_structure_helper_t3.h"

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
}  // namespace

constexpr int ScalabilityStructureHelperT3::kMaxNumSpatialLayers;
constexpr int ScalabilityStructureHelperT3::kMaxNumTemporalLayers;
constexpr absl::string_view ScalabilityStructureHelperT3::kFramePatternNames[];

ScalabilityStructureHelperT3::ScalabilityStructureHelperT3(
    int num_spatial_layers,
    int num_temporal_layers,
    ScalingFactor resolution_factor)
    : num_spatial_layers_(num_spatial_layers),
      num_temporal_layers_(num_temporal_layers),
      resolution_factor_(resolution_factor),
      active_decode_targets_(
          (uint32_t{1} << (num_spatial_layers * num_temporal_layers)) - 1) {
  RTC_DCHECK_LE(num_spatial_layers, kMaxNumSpatialLayers);
  RTC_DCHECK_LE(num_temporal_layers, kMaxNumTemporalLayers);
}

ScalableVideoController::StreamLayersConfig
ScalabilityStructureHelperT3::StreamConfig() const {
  ScalableVideoController::StreamLayersConfig result;
  result.num_spatial_layers = num_spatial_layers_;
  result.num_temporal_layers = num_temporal_layers_;
  result.scaling_factor_num[num_spatial_layers_ - 1] = 1;
  result.scaling_factor_den[num_spatial_layers_ - 1] = 1;
  for (int sid = num_spatial_layers_ - 1; sid > 0; --sid) {
    result.scaling_factor_num[sid - 1] =
        resolution_factor_.num * result.scaling_factor_num[sid];
    result.scaling_factor_den[sid - 1] =
        resolution_factor_.den * result.scaling_factor_den[sid];
  }
  return result;
}

bool ScalabilityStructureHelperT3::TemporalLayerIsActive(int tid) const {
  if (tid >= num_temporal_layers_) {
    return false;
  }
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    if (DecodeTargetIsActive(sid, tid)) {
      return true;
    }
  }
  return false;
}

ScalabilityStructureHelperT3::FramePattern
ScalabilityStructureHelperT3::NextPattern(
    ScalabilityStructureHelperT3::FramePattern last_pattern) const {
  switch (last_pattern) {
    case kNone:
    case kDeltaT2B:
      return kDeltaT0;
    case kDeltaT2A:
      if (TemporalLayerIsActive(1)) {
        return kDeltaT1;
      }
      return kDeltaT0;
    case kDeltaT1:
      if (TemporalLayerIsActive(2)) {
        return kDeltaT2B;
      }
      return kDeltaT0;
    case kDeltaT0:
      if (TemporalLayerIsActive(2)) {
        return kDeltaT2A;
      }
      if (TemporalLayerIsActive(1)) {
        return kDeltaT1;
      }
      return kDeltaT0;
  }
}

GenericFrameInfo ScalabilityStructureHelperT3::OnEncodeDone(
    const ScalableVideoController::LayerFrameConfig& config) {
  GenericFrameInfo frame_info;
  frame_info.spatial_id = config.SpatialId();
  frame_info.temporal_id = config.TemporalId();
  frame_info.encoder_buffers = config.Buffers();
  frame_info.decode_target_indications.reserve(num_spatial_layers_ *
                                               num_temporal_layers_);
  frame_info.part_of_chain.assign(num_spatial_layers_, false);
  frame_info.active_decode_targets = active_decode_targets_;
  return frame_info;
}

void ScalabilityStructureHelperT3::SetDecodeTargetsFromAllocation(
    const VideoBitrateAllocation& bitrates) {
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    // Enable/disable spatial layers independetely.
    bool active = true;
    for (int tid = 0; tid < num_temporal_layers_; ++tid) {
      // To enable temporal layer, require bitrates for lower temporal layers.
      active = active && bitrates.GetBitrate(sid, tid) > 0;
      SetDecodeTargetIsActive(sid, tid, active);
    }
  }
}

}  // namespace webrtc
