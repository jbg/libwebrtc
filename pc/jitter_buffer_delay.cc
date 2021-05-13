/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jitter_buffer_delay.h"

#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace {
constexpr int kDefaultDelay = 0;
constexpr int kMaximumDelayMs = 10000;
}  // namespace

namespace webrtc {

JitterBufferDelay::JitterBufferDelay() {
  worker_thread_checker_.Detach();
}

void JitterBufferDelay::OnStart(cricket::Delayable* media_channel,
                                absl::optional<uint32_t> ssrc) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // Trying to apply cached delay for the audio stream.
  if (cached_delay_seconds_) {
    Set(cached_delay_seconds_.value(), media_channel, ssrc);
  }
}

void JitterBufferDelay::Set(absl::optional<double> delay_seconds,
                            cricket::Delayable* media_channel,
                            absl::optional<uint32_t> ssrc) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  cached_delay_seconds_ = delay_seconds;
  if (media_channel && ssrc) {
    // TODO(kuddai) propagate absl::optional deeper down as default preference.
    int delay_ms =
        rtc::saturated_cast<int>(delay_seconds.value_or(kDefaultDelay) * 1000);
    delay_ms = rtc::SafeClamp(delay_ms, 0, kMaximumDelayMs);
    media_channel->SetBaseMinimumPlayoutDelayMs(ssrc.value(), delay_ms);
  }
}

}  // namespace webrtc
