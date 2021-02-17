/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_KEY_SVC_H_
#define MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_KEY_SVC_H_

#include <bitset>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_bitrate_allocation.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/scalability_structure_helper_t3.h"
#include "modules/video_coding/svc/scalable_video_controller.h"

namespace webrtc {

class ScalabilityStructureKeySvc : public ScalableVideoController {
 public:
  ScalabilityStructureKeySvc(int num_spatial_layers, int num_temporal_layers);
  ~ScalabilityStructureKeySvc() override;

  StreamLayersConfig StreamConfig() const override;

  std::vector<LayerFrameConfig> NextFrameConfig(bool restart) override;
  GenericFrameInfo OnEncodeDone(const LayerFrameConfig& config) override;
  void OnRatesUpdated(const VideoBitrateAllocation& bitrates) override;

 private:
  std::vector<LayerFrameConfig> KeyframeConfig();
  std::vector<LayerFrameConfig> T0Config();
  std::vector<LayerFrameConfig> T1Config();
  std::vector<LayerFrameConfig> T2Config();

  ScalabilityStructureHelperT3 helper_;
  ScalabilityStructureHelperT3::FramePattern last_pattern_ =
      ScalabilityStructureHelperT3::kNone;
  std::bitset<ScalabilityStructureHelperT3::kMaxNumSpatialLayers>
      spatial_id_is_enabled_;
  std::bitset<ScalabilityStructureHelperT3::kMaxNumSpatialLayers>
      can_reference_t1_frame_for_spatial_id_;
};

// S1  0--0--0-
//     |       ...
// S0  0--0--0-
class ScalabilityStructureL2T1Key : public ScalabilityStructureKeySvc {
 public:
  ScalabilityStructureL2T1Key() : ScalabilityStructureKeySvc(2, 1) {}
  ~ScalabilityStructureL2T1Key() override;

  FrameDependencyStructure DependencyStructure() const override;
};

// S1T1     0   0
//         /   /   /
// S1T0   0---0---0
//        |         ...
// S0T1   | 0   0
//        |/   /   /
// S0T0   0---0---0
// Time-> 0 1 2 3 4
class ScalabilityStructureL2T2Key : public ScalabilityStructureKeySvc {
 public:
  ScalabilityStructureL2T2Key() : ScalabilityStructureKeySvc(2, 2) {}
  ~ScalabilityStructureL2T2Key() override;

  FrameDependencyStructure DependencyStructure() const override;
};

class ScalabilityStructureL3T3Key : public ScalabilityStructureKeySvc {
 public:
  ScalabilityStructureL3T3Key() : ScalabilityStructureKeySvc(3, 3) {}
  ~ScalabilityStructureL3T3Key() override;

  FrameDependencyStructure DependencyStructure() const override;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_KEY_SVC_H_
