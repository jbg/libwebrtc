/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_processing.h"

#include "rtc_base/checks.h"

namespace webrtc {

ApmMessage::ApmMessage()
    : id_(Id::kNullMessage), bool_val_(false), type_(ValueType::kBool) {}

ApmMessage::~ApmMessage() = default;

bool ApmMessage::GetBool() const {
  RTC_DCHECK(type_ == ValueType::kBool);
  return bool_val_;
}

int ApmMessage::GetInt() const {
  RTC_DCHECK(type_ == ValueType::kInt);
  return int_val_;
}

float ApmMessage::GetFloat() const {
  RTC_DCHECK(type_ == ValueType::kFloat);
  return float_val_;
}

ApmMessage ApmMessage::CreateNullMessage(bool v) {
  return {Id::kNullMessage, v};
}

ApmMessage ApmMessage::CreateNullMessage(int v) {
  return {Id::kNullMessage, v};
}

ApmMessage ApmMessage::CreateNullMessage(float v) {
  return {Id::kNullMessage, v};
}

ApmMessage ApmMessage::CreateUpdateCapturePreGainMessage(float v) {
  return {Id::kUpdateCapturePreGain, v};
}

ApmMessage ApmMessage::CreateUpdateRenderGainMessage(float v) {
  return {Id::kUpdateRenderGain, v};
}

ApmMessage::ApmMessage(Id id, bool v)
    : id_(id), bool_val_(v), type_(ValueType::kBool) {}

ApmMessage::ApmMessage(Id id, int v)
    : id_(id), int_val_(v), type_(ValueType::kInt) {}

ApmMessage::ApmMessage(Id id, float v)
    : id_(id), float_val_(v), type_(ValueType::kFloat) {}

Beamforming::Beamforming()
    : enabled(false),
      array_geometry(),
      target_direction(
          SphericalPointf(static_cast<float>(M_PI) / 2.f, 0.f, 1.f)) {}
Beamforming::Beamforming(bool enabled, const std::vector<Point>& array_geometry)
    : Beamforming(enabled,
                  array_geometry,
                  SphericalPointf(static_cast<float>(M_PI) / 2.f, 0.f, 1.f)) {}

Beamforming::Beamforming(bool enabled,
                         const std::vector<Point>& array_geometry,
                         SphericalPointf target_direction)
    : enabled(enabled),
      array_geometry(array_geometry),
      target_direction(target_direction) {}

Beamforming::~Beamforming() {}

}  // namespace webrtc
