/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_
#define MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_

#include <cmath>
#include <vector>

#include "rtc_base/basictypes.h"

namespace webrtc {

constexpr float kMinSampleValue = -32768.f;
constexpr float kMaxSampleValue = 32767.f;

constexpr double kInputLevelScaling = 32768.0;
const double kMinDbfs = -20.0 * std::log10(32768.0);

// Limiter params.
constexpr double kLimiterMaxInputLevel = 1.0;
constexpr double kLimiterKneeSmoothness = 1.0;
constexpr double kLimiterCompressionRatio = 5.0;

// Number of interpolation points for each region of the limiter.
// These values have been tuned to limit the interpolated gain curve error given
// the limiter parameters and allowing a maximum error of +/- 32768^-1.
constexpr size_t kInterpolatedGainCurveKneePoints = 22;
constexpr size_t kInterpolatedGainCurveBeyondKneePoints = 10;

double DbfsToLinear(const double level);

double LinearToDbfs(const double level);

std::vector<double> LinSpace(const double l, const double r, size_t num_points);

// TODO(aleloi): add the other constants as more AGC2 components are
// added.
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_AGC2_COMMON_H_
