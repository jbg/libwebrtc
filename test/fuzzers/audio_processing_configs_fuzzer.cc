/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <bitset>
#include <string>

#include "api/audio/echo_canceller3_factory.h"
#include "modules/audio_processing/aec_dump/mock_aec_dump.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/field_trial_default.h"
#include "test/fuzzers/audio_processing_fuzzer_helper.h"
#include "test/fuzzers/fuzz_data_helper.h"

#include "modules/audio_processing/include/mock_audio_processing.h"

namespace webrtc {
//namespace // {

// const std::string kFieldTrialNames[] = {
//     "WebRTC-Aec3TransparentModeKillSwitch",
//     "WebRTC-Aec3StationaryRenderImprovementsKillSwitch",
//     "WebRTC-Aec3EnforceDelayAfterRealignmentKillSwitch",
//     "WebRTC-Aec3UseShortDelayEstimatorWindow",
//     "WebRTC-Aec3ReverbBasedOnRenderKillSwitch",
//     "WebRTC-Aec3ReverbModellingKillSwitch",
//     "WebRTC-Aec3FilterAnalyzerPreprocessorKillSwitch",
//     "WebRTC-Aec3TransparencyImprovementsKillSwitch",
//     "WebRTC-Aec3SoftTransparentModeKillSwitch",
//     "WebRTC-Aec3OverrideEchoPathGainKillSwitch",
//     "WebRTC-Aec3ZeroExternalDelayHeadroomKillSwitch",
//     "WebRTC-Aec3DownSamplingFactor8KillSwitch",
//     "WebRTC-Aec3EnforceSkewHysteresis1",
//     "WebRTC-Aec3EnforceSkewHysteresis2"};

// std::unique_ptr<AudioProcessing> CreateApm(test::FuzzDataHelper* fuzz_data,
//                                            std::string* field_trial_string) {
//   // Parse boolean values for optionally enabling different
//   // configurable public components of APM.
//   bool exp_agc = fuzz_data->ReadOrDefaultValue(true);
//   bool exp_ns = fuzz_data->ReadOrDefaultValue(true);
//   static_cast<void>(fuzz_data->ReadOrDefaultValue(true));
//   bool ef = fuzz_data->ReadOrDefaultValue(true);
//   bool raf = fuzz_data->ReadOrDefaultValue(true);
//   static_cast<void>(fuzz_data->ReadOrDefaultValue(true));
//   bool ie = fuzz_data->ReadOrDefaultValue(true);
//   bool red = fuzz_data->ReadOrDefaultValue(true);
//   bool hpf = fuzz_data->ReadOrDefaultValue(true);
//   bool aec3 = fuzz_data->ReadOrDefaultValue(true);

//   bool use_aec = fuzz_data->ReadOrDefaultValue(true);
//   bool use_aecm = fuzz_data->ReadOrDefaultValue(true);
//   bool use_agc = fuzz_data->ReadOrDefaultValue(true);
//   bool use_ns = fuzz_data->ReadOrDefaultValue(true);
//   bool use_le = fuzz_data->ReadOrDefaultValue(true);
//   bool use_vad = fuzz_data->ReadOrDefaultValue(true);
//   bool use_agc_limiter = fuzz_data->ReadOrDefaultValue(true);
//   bool use_agc2_limiter = fuzz_data->ReadOrDefaultValue(true);



//   static_cast<void>(exp_agc);
//   static_cast<void>(exp_ns);
//   static_cast<void>(fuzz_data->ReadOrDefaultValue(true));
//   static_cast<void>(ef);
//   static_cast<void>(raf);
//   static_cast<void>(fuzz_data->ReadOrDefaultValue(true));
//   static_cast<void>(ie);
//   static_cast<void>(red);
//   static_cast<void>(hpf);
//   static_cast<void>(aec3);

//   static_cast<void>(use_aec);
//   static_cast<void>(use_aecm);
//   static_cast<void>(use_agc);
//   static_cast<void>(use_ns);
//   static_cast<void>(use_le);
//   static_cast<void>(use_vad);
//   static_cast<void>(use_agc_limiter);
//   static_cast<void>(use_agc2_limiter);

//   // Read an int8 value, but don't let it be too large or small.
//   const float gain_controller2_gain_db =
//       rtc::SafeClamp<int>(fuzz_data->ReadOrDefaultValue<int8_t>(0), -50, 50);
//   static_cast<void>(gain_controller2_gain_db);

//   constexpr size_t kNumFieldTrials = arraysize(kFieldTrialNames);
//   // This check ensures the uint16_t that is read has enough bits to cover all
//   // the field trials.
//   RTC_DCHECK_LE(kNumFieldTrials, 16);
//   std::bitset<kNumFieldTrials> field_trial_bitmask(
//       fuzz_data->ReadOrDefaultValue<uint16_t>(0));
//   for (size_t i = 0; i < kNumFieldTrials; ++i) {
//     if (field_trial_bitmask[i]) {
//       *field_trial_string += kFieldTrialNames[i] + "/Enabled/";
//     }
//   }
//   //field_trial::InitFieldTrialsFromString(field_trial_string->c_str());

//   // Ignore a few bytes. Bytes from this segment will be used for
//   // future config flag changes. We assume 40 bytes is enough for
//   // configuring the APM.
//   constexpr size_t kSizeOfConfigSegment = 40;
//   RTC_DCHECK(kSizeOfConfigSegment >= fuzz_data->BytesRead());
//   static_cast<void>(
//       fuzz_data->ReadByteArray(kSizeOfConfigSegment - fuzz_data->BytesRead()));

//   // Filter out incompatible settings that lead to CHECK failures.
//   if (use_aecm && use_aec) {
//     return nullptr;
//   }

//   // Components can be enabled through webrtc::Config and
//   // webrtc::AudioProcessingConfig.

//   //apm->level_estimator()->Enable(use_le);
//   //apm->voice_detection()->Enable(use_vad);
//   //apm->gain_control()->enable_limiter(use_agc_limiter);

//   return apm;
// }
// }
// namespace

class MyEchoControlFactory : public EchoControlFactory {
 public:
  std::unique_ptr<EchoControl> Create(int sample_rate_hz) {
    auto ec = new test::MockEchoControl();
    EXPECT_CALL(*ec, AnalyzeRender(testing::_)).Times(1);
    EXPECT_CALL(*ec, AnalyzeCapture(testing::_)).Times(2);
    EXPECT_CALL(*ec, ProcessCapture(testing::_, testing::_)).Times(2);
    return std::unique_ptr<EchoControl>(ec);
  }
};

void FuzzOneInput(const uint8_t* data, size_t size) {
  // test::FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));
  // // This string must be in scope during execution, according to documentation
  // // for field_trial_default.h. Hence it's created here and not in CreateApm.
  // //std::string field_trial_string = "";
  // auto apm = CreateApm(&fuzz_data);

  AudioFrame fixed_frame;

  using Rate = AudioProcessing::NativeRate;
  // const Rate rate_kinds[] = {Rate::kSampleRate8kHz, Rate::kSampleRate16kHz,
  //                            Rate::kSampleRate32kHz, Rate::kSampleRate48kHz};

  fixed_frame.samples_per_channel_ = Rate::kSampleRate8kHz / 100;
  fixed_frame.sample_rate_hz_ = Rate::kSampleRate8kHz;
  fixed_frame.num_channels_ = 2;

  Config config;

  std::unique_ptr<EchoControlFactory> echo_control_factory;
  echo_control_factory.reset(new MyEchoControlFactory());
  std::unique_ptr<AudioProcessing> apm(
      AudioProcessingBuilder()
      .SetEchoControlFactory(std::move(echo_control_factory)
                                 )
          .Create());
  apm->echo_control_mobile()->Enable(true);
  apm->noise_suppression()->Enable(true);
  if (apm) {
    apm->ProcessStream(&fixed_frame);
  }

  // if (apm) {
  //   FuzzAudioProcessing(&fuzz_data, std::move(apm));
  // }
}
}  // namespace webrtc
