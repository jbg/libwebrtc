/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/to_be_removed.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {

void Foo(rtc::ArrayView<int> a) {
  RTC_CHECK_GT(a.size(), 0);
}

}  // namespace rnn_vad
}  // namespace webrtc
