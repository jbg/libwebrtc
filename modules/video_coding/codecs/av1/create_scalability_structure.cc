/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/create_scalability_structure.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t2.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t3.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1_key.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1h.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2_key.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2_key_shift.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l3t1.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l3t3.h"
#include "modules/video_coding/codecs/av1/scalability_structure_s2t1.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller_no_layering.h"

namespace webrtc {

std::unique_ptr<ScalableVideoController> CreateScalabilityStructure(
    absl::string_view name) {
  if (name == "L1T1" || name == "") {
    return std::make_unique<ScalableVideoControllerNoLayering>();
  }
  if (name == "L1T2") {
    return std::make_unique<ScalabilityStructureL1T2>();
  }
  if (name == "L1T3") {
    return std::make_unique<ScalabilityStructureL1T3>();
  }
  if (name == "L2T1") {
    return std::make_unique<ScalabilityStructureL2T1>();
  }
  if (name == "L2T1h") {
    return std::make_unique<ScalabilityStructureL2T1h>();
  }
  if (name == "L2T1Key") {
    // This structure is not present in the av1 spec
    return std::make_unique<ScalabilityStructureL2T1Key>();
  }
  if (name == "L2T2") {
    return std::make_unique<ScalabilityStructureL2T2>();
  }
  if (name == "L3T1") {
    return std::make_unique<ScalabilityStructureL3T1>();
  }
  if (name == "L3T3") {
    return std::make_unique<ScalabilityStructureL3T3>();
  }
  if (name == "S2T1") {
    return std::make_unique<ScalabilityStructureS2T1>();
  }
  // KEY and KEY_SHIFT structures require extra spatial and temporal ids
  // to be represented with av1 operation points.
  if (name == "L3T2_KEY" || name == "L2T2Key") {
    return std::make_unique<ScalabilityStructureL2T2Key>();
  }
  if (name == "L3T2_KEY_SHIFT" || name == "L2T2KeyShift") {
    return std::make_unique<ScalabilityStructureL2T2KeyShift>();
  }
  return nullptr;
}

}  // namespace webrtc
