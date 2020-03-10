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

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/source/playback_timing_processor/dynamic_range_integer.h"
#include "modules/rtp_rtcp/source/playback_timing_processor/playback_timing_processor.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
PlaybackTimingProcessor::PlaybackTimingProcessor(
    Clock* clock,
    TransportFeedbackSenderInterface* feedback_sender)
    : clock_(clock), feedback_sender_(feedback_sender) {
  module_thread_checker_.Detach();
}

int64_t PlaybackTimingProcessor::TimeUntilNextProcess() {
  RTC_DCHECK_RUN_ON(&module_thread_checker_);
  int64_t now = clock_->TimeInMilliseconds();
  if (now - last_process_time_ms_ < kProcessIntervalMs) {
    return last_process_time_ms_ + kProcessIntervalMs - now;
  }
  return 0;
}

void PlaybackTimingProcessor::Process() {
  RTC_DCHECK_RUN_ON(&module_thread_checker_);
  last_process_time_ms_ = clock_->TimeInMilliseconds();
  // TODO(kron): Add code that generates RTCP packets

  // Create packet with all unprocessed timings.
  std::cout << "Sending PlaybackTimingFeedback\n  ";
  // Send packet.
  std::map<uint32_t, std::vector<TimingInfo>> info_to_send;
  {
    rtc::CritScope lock(&timing_infos_crit_);
    info_to_send = std::move(timing_infos_);
    timing_infos_.clear();
  }

  for (auto timing_infos_ssrc : info_to_send) {
    std::cout << timing_infos_ssrc.first << ": ";
    for (auto timing_info : timing_infos_ssrc.second) {
      std::cout << timing_info.rtp_timestamp << "("
                << timing_info.decode_end - timing_info.decode_begin
                << " ms), ";
    }
    std::cout << "\n";
  }
}

// Methods for updating timing information.
void PlaybackTimingProcessor::UpdatePacketAndDecodeTiming(
    uint32_t ssrc,
    uint32_t rtp_timestamp,
    int64_t first_packet_received_us,
    int64_t last_packet_received_us,
    int64_t decode_begin_us,
    int64_t decode_end_us) {
  rtp_timestamp_last_decoded_ = rtp_timestamp;
  rtc::CritScope lock(&timing_infos_crit_);
  timing_infos_[ssrc].push_back({rtp_timestamp, first_packet_received_us,
                                 last_packet_received_us, decode_begin_us,
                                 decode_end_us});
}

}  // namespace webrtc.
