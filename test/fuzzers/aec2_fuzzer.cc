/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <cmath>

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/checks.h"

namespace webrtc {
std::unique_ptr<AudioProcessing> CreateAPM() {
  Config config;
  std::unique_ptr<AudioProcessing> apm(
      AudioProcessing::Create(config, nullptr, nullptr, nullptr));
  apm->echo_cancellation()->Enable(true);
  return apm;
}

void FuzzOneInput(const uint8_t* data, size_t size) {
  auto apm = CreateAPM();
  test::FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));
  FuzzAudioProcessing(&fuzz_data, std::move(apm));
}
}  // namespace webrtc
