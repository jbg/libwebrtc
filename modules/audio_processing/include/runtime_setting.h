/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_RUNTIME_SETTING_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_RUNTIME_SETTING_H_

#include "rtc_base/checks.h"

namespace webrtc {
// Specifies the properties of a setting to be passed to AudioProcessing at
// runtime.
class RuntimeSetting {
 public:
  enum class Type { kNotSpecified, kCapturePreGain };

  RuntimeSetting() : type_(Type::kNotSpecified), value_(0.f) {}
  ~RuntimeSetting() = default;

  static RuntimeSetting CreateCapturePreGain(float gain) {
    RTC_DCHECK_GE(gain, 1.f) << "Attenuation is not allowed.";
    return {Type::kCapturePreGain, gain};
  }

  Type type() const { return type_; }
  void GetFloat(float* value) const {
    RTC_DCHECK(value);
    *value = value_;
  }

 private:
  RuntimeSetting(Type id, float value) : type_(id), value_(value) {}
  Type type_;
  float value_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_RUNTIME_SETTING_H_
