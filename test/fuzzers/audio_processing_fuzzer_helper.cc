// /*
//  *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
//  *
//  *  Use of this source code is governed by a BSD-style license
//  *  that can be found in the LICENSE file in the root of the source
//  *  tree. An additional intellectual property rights grant can be found
//  *  in the file PATENTS.  All contributing project authors may
//  *  be found in the AUTHORS file in the root of the source tree.
//  */

// #include "test/fuzzers/audio_processing_fuzzer_helper.h"

// #include <algorithm>
// #include <array>
// #include <cmath>
// #include <limits>

// #include "api/audio/audio_frame.h"
// #include "modules/audio_processing/include/audio_processing.h"
// #include "rtc_base/checks.h"

// namespace webrtc {
// namespace {
// void GenerateFloatFrame(test::FuzzDataHelper* fuzz_data,
//                         size_t input_rate,
//                         size_t num_channels,
//                         float* const* float_frames) {
//   const size_t samples_per_input_channel =
//       rtc::CheckedDivExact(input_rate, 100ul);
//   RTC_DCHECK_LE(samples_per_input_channel, 480);
//   for (size_t i = 0; i < num_channels; ++i) {
//     for (size_t j = 0; j < samples_per_input_channel; ++j) {
//       float_frames[i][j] =
//           static_cast<float>(fuzz_data->ReadOrDefaultValue<int16_t>(0)) /
//           static_cast<float>(std::numeric_limits<int16_t>::max());
//     }
//   }
// }

// void GenerateFixedFrame(test::FuzzDataHelper* fuzz_data,
//                         size_t input_rate,
//                         size_t num_channels,
//                         AudioFrame* fixed_frame) {
//   const size_t samples_per_input_channel =
//       rtc::CheckedDivExact(input_rate, 100ul);
//   fixed_frame->samples_per_channel_ = samples_per_input_channel;
//   fixed_frame->sample_rate_hz_ = input_rate;
//   fixed_frame->num_channels_ = num_channels;

//   RTC_DCHECK_LE(samples_per_input_channel * num_channels,
//                 AudioFrame::kMaxDataSizeSamples);
//   for (size_t i = 0; i < samples_per_input_channel * num_channels; ++i) {
//     fixed_frame->mutable_data()[i] = fuzz_data->ReadOrDefaultValue<int16_t>(0);
//   }
// }
// }  // namespace

// void FuzzAudioProcessing(test::FuzzDataHelper* fuzz_data,
//                          std::unique_ptr<AudioProcessing> apm) {
//   AudioFrame fixed_frame;
//   std::array<float, 480> float_frame1;
//   std::array<float, 480> float_frame2;
//   std::array<float* const, 2> float_frame_ptrs = {
//       &float_frame1[0], &float_frame2[0],
//   };
//   float* const* ptr_to_float_frames = &float_frame_ptrs[0];

//   using Rate = AudioProcessing::NativeRate;
//   const Rate rate_kinds[] = {Rate::kSampleRate8kHz, Rate::kSampleRate16kHz,
//                              Rate::kSampleRate32kHz, Rate::kSampleRate48kHz};

//   // We may run out of fuzz data in the middle of a loop iteration. In
//   // that case, default values will be used for the rest of that
//   // iteration.
//   while (fuzz_data->CanReadBytes(1)) {
//     // // const
//     // bool is_float = fuzz_data->ReadOrDefaultValue(true);
//     // is_float = false;
//     // // Decide input/output rate for this iteration.
//     // // const
//     // auto input_rate =
//     //     static_cast<size_t>(fuzz_data->SelectOneOf(rate_kinds));
//     // input_rate = Rate::kSampleRate8kHz;
//     // // const
//     // auto output_rate =
//     //     static_cast<size_t>(fuzz_data->SelectOneOf(rate_kinds));
//     // output_rate = Rate::kSampleRate8kHz;

//     // /*const*/ int num_channels = fuzz_data->ReadOrDefaultValue(true) ? 2 : 1;
//     // num_channels = 2;
//     // const uint8_t stream_delay = fuzz_data->ReadOrDefaultValue<uint8_t>(0);
//     // static_cast<void>(stream_delay);

//     // API call needed for AEC-2 and AEC-m to run.
//     //apm->set_stream_delay_ms(stream_delay);

//     // Make the APM call depending on capture/render mode and float /
//     // fix interface.
//     // const
//     // bool is_capture = fuzz_data->ReadOrDefaultValue(true);
//     // is_capture = true;

//     fixed_frame.samples_per_channel_ = Rate::kSampleRate8kHz / 100;
//     fixed_frame.sample_rate_hz_ = Rate::kSampleRate8kHz;
//     fixed_frame.num_channels_ = 2;
//     apm->ProcessStream(&fixed_frame);


//     // Make calls to stats gathering functions to cover these
//     // codeways.
//     // static_cast<void>(apm->GetStatistics());
//     // static_cast<void>(apm->GetStatistics(true));

//     //RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
//   }
// }
// }  // namespace webrtc
