/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/audio_processing_fuzzer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/checks.h"

namespace webrtc {
void FuzzAudioProcessing(test::FuzzDataHelper* fuzz_data,
                         std::unique_ptr<AudioProcessing> apm) {
  AudioFrame fixed_frame;
  std::array<float, 480> float_frame{};
  float* const first_channel = &float_frame[0];

  using Rate = AudioProcessing::NativeRate;
  const Rate rate_kinds[] = {Rate::kSampleRate8kHz, Rate::kSampleRate16kHz,
                             Rate::kSampleRate32kHz, Rate::kSampleRate48kHz};

  while (fuzz_data->CanReadBytes(1)) {
    const bool is_float = fuzz_data->ReadOrDefaultValue(true);
    // Decide input/output rate for this iteration.
    const auto input_rate =
        static_cast<size_t>(fuzz_data->SelectOneOf(rate_kinds));
    const auto output_rate =
        static_cast<size_t>(fuzz_data->SelectOneOf(rate_kinds));

    const bool use_stereo = fuzz_data->ReadOrDefaultValue(true);
    const uint8_t stream_delay = fuzz_data->ReadOrDefaultValue(0);

    const size_t samples_per_input_channel =
        rtc::CheckedDivExact(input_rate, 100ul);
    fixed_frame.samples_per_channel_ = samples_per_input_channel;
    fixed_frame.sample_rate_hz_ = input_rate;

    // Two channels breaks AEC3.
    fixed_frame.num_channels_ = use_stereo ? 1 : 2;

    // Fill the arrays with audio samples from the data.
    if (is_float) {
      for (size_t i = 0; i < fixed_frame.samples_per_channel_; ++i) {
        float_frame[i] = fuzz_data->ReadOrDefaultValue<int16_t>(0);
      }
    } else {
      for (size_t i = 0;
           i < fixed_frame.samples_per_channel_ * fixed_frame.num_channels_;
           ++i) {
        fixed_frame.mutable_data()[i] =
            fuzz_data->ReadOrDefaultValue(fixed_frame.mutable_data()[i]);
      }
    }

    // Filter obviously wrong values like inf/nan and values that will
    // lead to inf/nan in calculations. 1e6 leads to DCHECKS failing.
    for (auto& x : float_frame) {
      if (!std::isnormal(x) || std::abs(x) > 1e5) {
        x = 0;
      }
    }

    apm->set_stream_delay_ms(stream_delay);

    // Make the APM call depending on capture/render mode and float /
    // fix interface.
    const bool is_capture = fuzz_data->ReadOrDefaultValue(true);

    if (is_capture) {
      auto apm_return_code =
          is_float ? (apm->ProcessStream(
                         &first_channel, StreamConfig(input_rate, 1),
                         StreamConfig(output_rate, 1), &first_channel))
                   : (apm->ProcessStream(&fixed_frame));
      RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
    } else {
      auto apm_return_code =
          is_float ? (apm->ProcessReverseStream(
                         &first_channel, StreamConfig(input_rate, 1),
                         StreamConfig(output_rate, 1), &first_channel))
                   : (apm->ProcessReverseStream(&fixed_frame));
      RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
    }
  }
}
}  // namespace webrtc
