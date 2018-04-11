/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/gain_applier.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "rtc_base/gunit.h"

namespace webrtc {
TEST(AutomaticGainController2GainApplier, InitialGainIsRespected) {
  const float initial_signal_level = 123.f;
  const float gain_factor = 10.f;
  VectorFloatFrame fake_audio(1, 1, initial_signal_level);
  GainApplier gain_applier(true, gain_factor);

  gain_applier.ApplyGain(fake_audio.float_frame_view());
  EXPECT_NEAR(fake_audio.float_frame_view().channel(0)[0],
              initial_signal_level * gain_factor, 0.1f);
}

TEST(AutomaticGainController2GainApplier, ClippingIsDone) {
  const float initial_signal_level = 30000.f;
  const float gain_factor = 10.f;
  VectorFloatFrame fake_audio(1, 1, initial_signal_level);
  GainApplier gain_applier(true, gain_factor);

  gain_applier.ApplyGain(fake_audio.float_frame_view());
  EXPECT_NEAR(fake_audio.float_frame_view().channel(0)[0],
              std::numeric_limits<int16_t>::max(), 0.1f);
}

TEST(AutomaticGainController2GainApplier, ClippingIsNotDone) {
  const float initial_signal_level = 30000.f;
  const float gain_factor = 10.f;
  VectorFloatFrame fake_audio(1, 1, initial_signal_level);
  GainApplier gain_applier(false, gain_factor);

  gain_applier.ApplyGain(fake_audio.float_frame_view());

  EXPECT_NEAR(fake_audio.float_frame_view().channel(0)[0],
              initial_signal_level * gain_factor, 0.1f);
}

TEST(AutomaticGainController2GainApplier, RampingIsDone) {
  const float initial_signal_level = 30000.f;
  const float initial_gain_factor = 1.f;
  const float target_gain_factor = 0.5f;
  const int num_channels = 3;
  const int samples_per_channel = 4;
  VectorFloatFrame fake_audio(num_channels, samples_per_channel,
                              initial_signal_level);
  GainApplier gain_applier(false, initial_gain_factor);

  gain_applier.SetGainFactor(target_gain_factor);
  gain_applier.ApplyGain(fake_audio.float_frame_view());

  // The maximal gain change should be close to that in linear interpolation.
  for (size_t channel = 0; channel < num_channels; ++channel) {
    float max_signal_change = 0.f;
    float last_signal_level = initial_signal_level;
    for (auto sample : fake_audio.float_frame_view().channel(channel)) {
      max_signal_change =
          std::max(max_signal_change, fabs(last_signal_level - sample));
      last_signal_level = sample;
    }
    const float total_gain_change =
        fabs((initial_gain_factor - target_gain_factor) * initial_signal_level);
    EXPECT_NEAR(max_signal_change, total_gain_change / samples_per_channel,
                0.1f);
  }

  // Next frame should have the desired level.
  VectorFloatFrame next_fake_audio_frame(num_channels, samples_per_channel,
                                         initial_signal_level);
  gain_applier.ApplyGain(next_fake_audio_frame.float_frame_view());

  // The last sample should have the new gain.
  EXPECT_NEAR(next_fake_audio_frame.float_frame_view().channel(0)[0],
              initial_signal_level * target_gain_factor, 0.1f);
}
}  // namespace webrtc
