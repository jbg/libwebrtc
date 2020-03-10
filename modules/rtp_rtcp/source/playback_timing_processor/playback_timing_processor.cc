/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <utility>

#include "modules/rtp_rtcp/source/playback_timing_processor/playback_timing_processor.h"

namespace webrtc {
constexpr TimeDelta PlaybackTimingProcessor::kProcessInterval;

PlaybackTimingProcessor::PlaybackTimingProcessor(
    Clock* clock,
    TransportFeedbackSenderInterface* feedback_sender)
    : clock_(clock),
      feedback_sender_(feedback_sender),
      last_process_time_(Timestamp::Millis(0)) {
  module_sequence_checker_.Detach();
}

int64_t PlaybackTimingProcessor::TimeUntilNextProcess() {
  RTC_DCHECK_RUN_ON(&module_sequence_checker_);
  Timestamp now = clock_->CurrentTime();
  if (now - last_process_time_ < kProcessInterval) {
    return (last_process_time_ + kProcessInterval - now).ms();
  }
  return 0;
}

void PlaybackTimingProcessor::Process() {
  RTC_DCHECK_RUN_ON(&module_sequence_checker_);
  last_process_time_ = clock_->CurrentTime();
  // TODO(kron): Add code that generates RTCP packets

  // Create packet with all unprocessed timings.
  std::cout << "Sending PlaybackTimingFeedback\n  ";
  // Send packet.
  std::map<uint32_t, std::vector<TimingInfo>> info_to_send;
  {
    rtc::CritScope lock(&timing_infos_crit_);
    info_to_send.swap(timing_infos_);
  }

  for (auto timing_infos_ssrc : info_to_send) {
    std::cout << timing_infos_ssrc.first << ": ";
    for (auto timing_info : timing_infos_ssrc.second) {
      std::cout << timing_info.rtp_timestamp << "("
                << (timing_info.decode_end - timing_info.decode_begin).ms()
                << " ms), ";
    }
    std::cout << "\n";
  }
}

// Methods for updating timing information.
void PlaybackTimingProcessor::UpdatePacketAndDecodeTiming(
    uint32_t ssrc,
    const TimingInfo& timing_info) {
  rtc::CritScope lock(&timing_infos_crit_);
  rtp_timestamp_last_decoded_ = timing_info.rtp_timestamp;
  timing_infos_[ssrc].push_back(timing_info);
}

}  // namespace webrtc.
