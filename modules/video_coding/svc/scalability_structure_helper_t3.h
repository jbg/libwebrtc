/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_HELPER_T3_H_
#define MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_HELPER_T3_H_

#include <string>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_bitrate_allocation.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/scalable_video_controller.h"

namespace webrtc {

// Helper class for structures with the same temporal pattern across different
// spatial layers for up to 3 temporal layer.
// i.e. common code for LxTy, LxTy_KEY and SxTy structures with y<=3.
class ScalabilityStructureHelperT3 {
 public:
  enum FramePattern {
    kNone,
    kDeltaT2A,
    kDeltaT1,
    kDeltaT2B,
    kDeltaT0,
  };
  struct ScalingFactor {
    int num = 1;
    int den = 2;
  };

  static constexpr absl::string_view kFramePatternNames[] = {
      "None", "DeltaT2A", "DeltaT1", "DeltaT2B", "DeltaT0"};
  static constexpr int kMaxNumSpatialLayers = 3;
  static constexpr int kMaxNumTemporalLayers = 3;

  ScalabilityStructureHelperT3(int num_spatial_layers,
                               int num_temporal_layers,
                               ScalingFactor resolution_factor);
  ScalabilityStructureHelperT3(const ScalabilityStructureHelperT3&) = delete;
  ScalabilityStructureHelperT3& operator=(const ScalabilityStructureHelperT3&) =
      delete;
  ~ScalabilityStructureHelperT3() = default;

  ScalableVideoController::StreamLayersConfig StreamConfig() const;

  GenericFrameInfo OnEncodeDone(
      const ScalableVideoController::LayerFrameConfig& config);
  void SetDecodeTargetsFromAllocation(const VideoBitrateAllocation& bitrates);

  // Index of the buffer to store last frame for layer (`sid`, `tid`)
  int BufferIndex(int sid, int tid) const {
    return tid * num_spatial_layers_ + sid;
  }
  bool AnyDecodeTargetIsActive() const { return active_decode_targets_.any(); }
  bool DecodeTargetIsActive(int sid, int tid) const {
    return active_decode_targets_[sid * num_temporal_layers_ + tid];
  }
  void SetDecodeTargetIsActive(int sid, int tid, bool value) {
    active_decode_targets_.set(sid * num_temporal_layers_ + tid, value);
  }
  FramePattern NextPattern(FramePattern last_pattern) const;
  bool TemporalLayerIsActive(int tid) const;

  int num_spatial_layers() const { return num_spatial_layers_; }
  int num_temporal_layers() const { return num_temporal_layers_; }
  bool AnyActiveDecodeTargets() const { return active_decode_targets_.any(); }
  std::string PrintableDecodeTargets() const {
    return active_decode_targets_.to_string('-').substr(
        active_decode_targets_.size() -
        num_spatial_layers_ * num_temporal_layers_);
  }

 private:
  const int num_spatial_layers_;
  const int num_temporal_layers_;
  const ScalingFactor resolution_factor_;

  std::bitset<32> active_decode_targets_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_HELPER_T3_H_
