/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/stats.h"

#include <stdio.h>

#include <algorithm>

#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"

namespace webrtc {
namespace test {

FrameStatistic* Stats::AddFrame() {
  // We don't expect more frames than what can be stored in an int.
  stats_.emplace_back(static_cast<int>(stats_.size()));
  return &stats_.back();
}

FrameStatistic* Stats::GetFrame(int frame_number) {
  RTC_CHECK_GE(frame_number, 0);
  RTC_CHECK_LT(frame_number, stats_.size());
  return &stats_[frame_number];
}

size_t Stats::size() const {
  return stats_.size();
}

}  // namespace test
}  // namespace webrtc
