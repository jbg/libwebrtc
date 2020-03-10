/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This class sends playback timing information through RTCP packets.

#ifndef MODULES_RTP_RTCP_SOURCE_PLAYBACK_TIMING_PROCESSOR_PLAYBACK_TIMING_PROCESSOR_H_
#define MODULES_RTP_RTCP_SOURCE_PLAYBACK_TIMING_PROCESSOR_PLAYBACK_TIMING_PROCESSOR_H_

#include <map>
#include <memory>
#include <vector>

#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class PlaybackTimingCallback {
 public:
  virtual ~PlaybackTimingCallback() = default;
  virtual void UpdatePacketAndDecodeTiming(uint32_t ssrc,
                                           uint32_t rtp_timestmap,
                                           int64_t first_packet_received,
                                           int64_t last_packet_received,
                                           int64_t decode_begin,
                                           int64_t decode_end) = 0;
};

class Clock;
class TransportFeedbackSenderInterface;
class PlaybackTimingProcessor : public Module, public PlaybackTimingCallback {
 public:
  struct TimingInfo {
    uint32_t rtp_timestamp;
    int64_t first_packet_received;
    int64_t last_packet_received;
    int64_t decode_begin;
    int64_t decode_end;
  };

  PlaybackTimingProcessor(Clock* clock,
                          TransportFeedbackSenderInterface* feedback_sender);

  // Implement Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Methods for updating timing information.
  void UpdatePacketAndDecodeTiming(uint32_t ssrc,
                                   uint32_t rtp_timestmap,
                                   int64_t first_packet_received,
                                   int64_t last_packet_received,
                                   int64_t decode_begin,
                                   int64_t decode_end) override;

 protected:
  rtc::ThreadChecker module_thread_checker_;
  static const int64_t kProcessIntervalMs = 500;
  static const int64_t kStreamTimeOutMs = 2000;
  Clock* clock_;
  TransportFeedbackSenderInterface* const feedback_sender_;
  int64_t last_process_time_ms_ RTC_GUARDED_BY(module_thread_checker_) = 0;
  rtc::CriticalSection timing_infos_crit_;
  std::map<uint32_t, std::vector<TimingInfo>> timing_infos_
      RTC_GUARDED_BY(&timing_infos_crit_);
  uint32_t rtp_timestamp_last_decoded_;
  uint32_t rtp_timestamp_last_presented_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_PLAYBACK_TIMING_PROCESSOR_PLAYBACK_TIMING_PROCESSOR_H_
