/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/ns/fast_math.h"

#include <stdint.h>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

float FastLog2f(float in) {
  // TODO(peah): Add fast approximate implementation.
  RTC_DCHECK_GT(in, .0f);
  return log2(in);
}

}  // namespace

float SqrtFastApproximation(float f) {
  union {
    float f;
    uint32_t i;
  } u = {.f = f};
  u.i = (u.i + (127 << 23)) >> 1;
  return u.f;

  return sqrtf(f);
}

float Pow2Approximation(float p) {
  // TODO(peah): Add fast approximate implementation.
  return powf(2.f, p);
}

float PowApproximation(float x, float p) {
  return Pow2Approximation(p * FastLog2f(x));
}

float LogApproximation(float x) {
  return FastLog2f(x) * 0.69314718056f;
}

void LogApproximation(rtc::ArrayView<const float> x, rtc::ArrayView<float> y) {
  for (size_t k = 0; k < x.size(); ++k) {
    y[k] = LogApproximation(x[k]);
  }
}

float ExpApproximation(float x) {
  return PowApproximation(10.f, x * 0.4342944819f);
}

void ExpApproximation(rtc::ArrayView<const float> x, rtc::ArrayView<float> y) {
  for (size_t k = 0; k < x.size(); ++k) {
    y[k] = ExpApproximation(x[k]);
  }
}

void ExpApproximationSignFlip(rtc::ArrayView<const float> x,
                              rtc::ArrayView<float> y) {
  for (size_t k = 0; k < x.size(); ++k) {
    y[k] = ExpApproximation(-x[k]);
  }
}

}  // namespace webrtc
