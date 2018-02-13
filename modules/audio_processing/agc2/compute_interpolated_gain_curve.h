/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_COMPUTE_INTERPOLATED_GAIN_CURVE_H_
#define MODULES_AUDIO_PROCESSING_AGC2_COMPUTE_INTERPOLATED_GAIN_CURVE_H_

#include <array>

#include "modules/audio_processing/agc2/interpolated_gain_curve.h"

namespace webrtc {

namespace test {

// Parameters for interpolated gain curve using under-approximation to
// avoid saturation.
//
// The saturation gain is defined in order to let hard-clipping occur for
// those samples having a level that falls in the saturation region. It is an
// upper bound of the actual gain to apply - i.e., that returned by the
// limiter.

// Knee and beyond-knee regions approximation parameters.
// The gain curve is approximated as a piece-wise linear function.
// |approx_params_x_| are the boundaries between adjacent linear pieces,
// |approx_params_m_| and |approx_params_q_| are the slope and the y-intercept
// values of each piece.
extern std::array<float, kInterpolatedGainCurveTotalPoints>
    computed_approximation_params_x;
extern std::array<float, kInterpolatedGainCurveTotalPoints>
    computed_approximation_params_m;
extern std::array<float, kInterpolatedGainCurveTotalPoints>
    computed_approximation_params_q;

// Fills the arrays 'computed_approximation_params_*'.
void ComputeInterpolatedGainCurveApproximationParams();
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_COMPUTE_INTERPOLATED_GAIN_CURVE_H_
