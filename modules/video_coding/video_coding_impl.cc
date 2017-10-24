/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_coding_impl.h"

#include <algorithm>
#include <utility>

#include "common_types.h"  // NOLINT(build/include)
#include "common_video/include/video_bitrate_allocator.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/jitter_buffer.h"
#include "modules/video_coding/packet.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace vcm {

int64_t VCMProcessTimer::Period() const {
  return _periodMs;
}

int64_t VCMProcessTimer::TimeUntilProcess() const {
  const int64_t time_since_process = _clock->TimeInMilliseconds() - _latestMs;
  const int64_t time_until_process = _periodMs - time_since_process;
  return std::max<int64_t>(time_until_process, 0);
}

void VCMProcessTimer::Processed() {
  _latestMs = _clock->TimeInMilliseconds();
}
}  // namespace vcm

}  // namespace webrtc
