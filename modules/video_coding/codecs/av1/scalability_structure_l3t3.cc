/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_l3t3.h"

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
constexpr auto kRequired = DecodeTargetIndication::kRequired;

constexpr DecodeTargetIndication kDtis[12][9] = {
    // Key, S0
    {kSwitch, kSwitch, kSwitch,   // S0
     kSwitch, kSwitch, kSwitch,   // S1
     kSwitch, kSwitch, kSwitch},  // S2
    // Key, S1
    {kNotPresent, kNotPresent, kNotPresent,  // S0
     kSwitch, kSwitch, kSwitch,              // S1
     kSwitch, kSwitch, kSwitch},             // S2
    // Key, S2
    {kNotPresent, kNotPresent, kNotPresent,  // S0
     kNotPresent, kNotPresent, kNotPresent,  // S1
     kSwitch, kSwitch, kSwitch},             // S2
    // Delta, S0T0
    {kSwitch, kSwitch, kSwitch,         // S0
     kRequired, kRequired, kRequired,   // S1
     kRequired, kRequired, kRequired},  // S2
    // Delta, S1T0
    {kNotPresent, kNotPresent, kNotPresent,  // S0
     kSwitch, kSwitch, kSwitch,              // S1
     kRequired, kRequired, kRequired},       // S2
    // Delta, S2T0
    {kNotPresent, kNotPresent, kNotPresent,  // S0
     kNotPresent, kNotPresent, kNotPresent,  // S1
     kSwitch, kSwitch, kSwitch},             // S2
    // Delta, S0T1
    {kNotPresent, kDiscardable, kSwitch,  // S0
     kNotPresent, kRequired, kRequired,   // S1
     kNotPresent, kRequired, kRequired},  // S2
    // Delta, S1T1
    {kNotPresent, kNotPresent, kNotPresent,  // S0
     kNotPresent, kDiscardable, kSwitch,     // S1
     kNotPresent, kRequired, kRequired},     // S2
    // Delta, S2T1
    {kNotPresent, kNotPresent, kNotPresent,  // S0
     kNotPresent, kNotPresent, kNotPresent,  // S1
     kNotPresent, kDiscardable, kSwitch},    // S2
    // Delta, S0T2
    {kNotPresent, kNotPresent, kDiscardable,  // S0
     kNotPresent, kNotPresent, kRequired,     // S1
     kNotPresent, kNotPresent, kRequired},    // S2
    // Delta, S1T2
    {kNotPresent, kNotPresent, kNotPresent,   // S0
     kNotPresent, kNotPresent, kDiscardable,  // S1
     kNotPresent, kNotPresent, kRequired},    // S2
    // Delta, S2T2
    {kNotPresent, kNotPresent, kNotPresent,    // S0
     kNotPresent, kNotPresent, kNotPresent,    // S1
     kNotPresent, kNotPresent, kDiscardable},  // S2
};

}  // namespace

ScalabilityStructureL3T3::~ScalabilityStructureL3T3() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureL3T3::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 3;
  result.num_temporal_layers = 3;
  return result;
}

FrameDependencyStructure ScalabilityStructureL3T3::DependencyStructure() const {
  using Builder = GenericFrameInfo::Builder;
  FrameDependencyStructure structure;
  structure.num_decode_targets = 9;
  structure.num_chains = 3;
  structure.decode_target_protected_by_chain = {0, 0, 0, 1, 1, 1, 2, 2, 2};
  structure.templates = {
      Builder().S(0).T(0).Dtis("SSSSSSSSS").ChainDiffs({0, 0, 0}).Build(),
      Builder()
          .S(0)
          .T(0)
          .Dtis("SSSRRRRRR")
          .Fdiffs({12})
          .ChainDiffs({12, 11, 10})
          .Build(),
      Builder()
          .S(0)
          .T(1)
          .Dtis("-DS-RR-RR")
          .Fdiffs({6})
          .ChainDiffs({6, 5, 4})
          .Build(),
      Builder()
          .S(0)
          .T(2)
          .Dtis("--D--R--R")
          .Fdiffs({3})
          .ChainDiffs({3, 2, 1})
          .Build(),
      Builder()
          .S(0)
          .T(2)
          .Dtis("--D--R--R")
          .Fdiffs({3})
          .ChainDiffs({9, 8, 7})
          .Build(),
      Builder()
          .S(1)
          .T(0)
          .Dtis("---SSSSSS")
          .Fdiffs({1})
          .ChainDiffs({1, 1, 1})
          .Build(),
      Builder()
          .S(1)
          .T(0)
          .Dtis("---SSSRRR")
          .Fdiffs({12, 1})
          .ChainDiffs({1, 1, 1})
          .Build(),
      Builder()
          .S(1)
          .T(1)
          .Dtis("----DS-RR")
          .Fdiffs({6, 1})
          .ChainDiffs({7, 6, 5})
          .Build(),
      Builder()
          .S(1)
          .T(2)
          .Dtis("-----D--R")
          .Fdiffs({3, 1})
          .ChainDiffs({4, 3, 2})
          .Build(),
      Builder()
          .S(1)
          .T(2)
          .Dtis("-----D--R")
          .Fdiffs({3, 1})
          .ChainDiffs({10, 9, 8})
          .Build(),
      Builder()
          .S(2)
          .T(0)
          .Dtis("------SSS")
          .Fdiffs({1})
          .ChainDiffs({2, 1, 1})
          .Build(),
      Builder()
          .S(2)
          .T(0)
          .Dtis("------SSS")
          .Fdiffs({12, 1})
          .ChainDiffs({2, 1, 1})
          .Build(),
      Builder()
          .S(2)
          .T(1)
          .Dtis("-------DS")
          .Fdiffs({6, 1})
          .ChainDiffs({8, 7, 6})
          .Build(),
      Builder()
          .S(2)
          .T(2)
          .Dtis("--------D")
          .Fdiffs({3, 1})
          .ChainDiffs({5, 4, 3})
          .Build(),
      Builder()
          .S(2)
          .T(2)
          .Dtis("--------D")
          .Fdiffs({3, 1})
          .ChainDiffs({11, 10, 9})
          .Build(),
  };
  return structure;
}

ScalableVideoController::LayerFrameConfig
ScalabilityStructureL3T3::KeyFrameConfig() const {
  LayerFrameConfig result;
  result.id = 0;
  result.is_keyframe = true;
  result.spatial_id = 0;
  result.temporal_id = 0;
  result.buffers = {{/*id=*/0, /*referenced=*/false, /*updated=*/true}};
  return result;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureL3T3::NextFrameConfig(bool restart) {
  if (restart) {
    next_pattern_ = kKeyFrame;
  }
  std::vector<LayerFrameConfig> result(3);

  // For this structure name each of 8 buffers after the layer of the frame that
  // buffer keeps.
  static constexpr int kS0T0 = 0;
  static constexpr int kS1T0 = 1;
  static constexpr int kS2T0 = 2;
  static constexpr int kS0T1 = 3;
  static constexpr int kS1T1 = 4;
  static constexpr int kS2T1 = 5;
  static constexpr int kS0T2 = 6;
  static constexpr int kS1T2 = 7;
  switch (next_pattern_) {
    case kKeyFrame:
      result[0] = KeyFrameConfig();

      result[1].id = 1;
      result[1].spatial_id = 1;
      result[1].temporal_id = 0;
      result[1].buffers = {{kS1T0, /*referenced=*/false, /*updated=*/true},
                           {kS0T0, /*referenced=*/true, /*updated=*/false}};

      result[2].id = 2;
      result[2].spatial_id = 2;
      result[2].temporal_id = 0;
      result[2].buffers = {{kS2T0, /*referenced=*/false, /*updated=*/true},
                           {kS1T0, /*referenced=*/true, /*updated=*/false}};

      next_pattern_ = kDeltaFrameT2A;
      break;
    case kDeltaFrameT2A:
      result[0].id = 9;
      result[0].spatial_id = 0;
      result[0].temporal_id = 2;
      result[0].buffers = {{kS0T0, /*referenced=*/true, /*updated=*/false},
                           {kS0T2, /*referenced=*/false, /*updated=*/true}};

      result[1].id = 10;
      result[1].spatial_id = 1;
      result[1].temporal_id = 2;
      result[1].buffers = {{kS1T0, /*referenced=*/true, /*updated=*/false},
                           {kS0T2, /*referenced=*/true, /*updated=*/false},
                           {kS1T2, /*referenced=*/false, /*updated=*/true}};

      result[2].id = 11;
      result[2].spatial_id = 2;
      result[2].temporal_id = 2;
      result[2].buffers = {{kS2T0, /*referenced=*/true, /*updated=*/false},
                           {kS1T2, /*referenced=*/true, /*updated=*/false}};
      next_pattern_ = kDeltaFrameT1;
      break;
    case kDeltaFrameT1:
      result[0].id = 6;
      result[0].spatial_id = 0;
      result[0].temporal_id = 1;
      result[0].buffers = {{kS0T0, /*referenced=*/true, /*updated=*/false},
                           {kS0T1, /*referenced=*/false, /*updated=*/true}};

      result[1].id = 7;
      result[1].spatial_id = 1;
      result[1].temporal_id = 1;
      result[1].buffers = {{kS1T0, /*referenced=*/true, /*updated=*/false},
                           {kS0T1, /*referenced=*/true, /*updated=*/false},
                           {kS1T1, /*referenced=*/false, /*updated=*/true}};

      result[2].id = 8;
      result[2].spatial_id = 2;
      result[2].temporal_id = 1;
      result[2].buffers = {{kS2T0, /*referenced=*/true, /*updated=*/false},
                           {kS1T1, /*referenced=*/true, /*updated=*/false},
                           {kS2T1, /*referenced=*/false, /*updated=*/true}};
      next_pattern_ = kDeltaFrameT2B;
      break;
    case kDeltaFrameT2B:
      result[0].id = 9;
      result[0].spatial_id = 0;
      result[0].temporal_id = 2;
      result[0].buffers = {{kS0T1, /*referenced=*/true, /*updated=*/false},
                           {kS0T2, /*referenced=*/false, /*updated=*/true}};

      result[1].id = 10;
      result[1].spatial_id = 1;
      result[1].temporal_id = 2;
      result[1].buffers = {{kS1T1, /*referenced=*/true, /*updated=*/false},
                           {kS0T2, /*referenced=*/true, /*updated=*/false},
                           {kS1T2, /*referenced=*/false, /*updated=*/true}};

      result[2].id = 11;
      result[2].spatial_id = 2;
      result[2].temporal_id = 2;
      result[2].buffers = {{kS2T1, /*referenced=*/true, /*updated=*/false},
                           {kS1T2, /*referenced=*/true, /*updated=*/false}};
      next_pattern_ = kDeltaFrameT0;
      break;
    case kDeltaFrameT0:
      result[0].id = 3;
      result[0].spatial_id = 0;
      result[0].temporal_id = 0;
      result[0].buffers = {{kS0T0, /*referenced=*/true, /*updated=*/true}};

      result[1].id = 4;
      result[1].spatial_id = 1;
      result[1].temporal_id = 0;
      result[1].buffers = {{kS1T0, /*referenced=*/true, /*updated=*/true},
                           {kS0T0, /*referenced=*/true, /*updated=*/false}};

      result[2].id = 5;
      result[2].spatial_id = 2;
      result[2].temporal_id = 0;
      result[2].buffers = {{kS2T0, /*referenced=*/true, /*updated=*/true},
                           {kS1T0, /*referenced=*/true, /*updated=*/false}};

      next_pattern_ = kDeltaFrameT2A;
      break;
  }
  return result;
}

absl::optional<GenericFrameInfo> ScalabilityStructureL3T3::OnEncodeDone(
    LayerFrameConfig config) {
  if (config.is_keyframe) {
    config = KeyFrameConfig();
  }

  absl::optional<GenericFrameInfo> frame_info;
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
  if (config.temporal_id == 0) {
    frame_info->part_of_chain = {config.spatial_id == 0, config.spatial_id <= 1,
                                 true};
  } else {
    frame_info->part_of_chain = {false, false, false};
  }
  return frame_info;
}

}  // namespace webrtc
