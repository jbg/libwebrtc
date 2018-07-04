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

  AudioFrame fixed_frame;

  using Rate = AudioProcessing::NativeRate;

  fixed_frame.samples_per_channel_ = Rate::kSampleRate8kHz / 100;
  fixed_frame.sample_rate_hz_ = Rate::kSampleRate8kHz;
  fixed_frame.num_channels_ = 2;

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
}
}  // namespace webrtc
