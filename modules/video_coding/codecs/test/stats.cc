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
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

FrameStatistic* Stats::AddFrame() {
  stats_.emplace_back(stats_.size());
  return &stats_.back();
}

FrameStatistic* Stats::GetFrame(size_t frame_number) {
  RTC_CHECK_LT(frame_number, stats_.size());
  return &stats_[frame_number];
}

size_t Stats::size() const {
  return stats_.size();
}

}  // namespace test
}  // namespace webrtc
