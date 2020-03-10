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
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class PlaybackTimingCallback {
 public:
  struct TimingInfo {
    uint32_t rtp_timestamp;
    Timestamp first_packet_received;
    Timestamp last_packet_received;
    Timestamp decode_begin;
    Timestamp decode_end;
  };
  virtual ~PlaybackTimingCallback() = default;
  virtual void UpdatePacketAndDecodeTiming(uint32_t ssrc,
                                           const TimingInfo& timing_info) = 0;
};

// TODO(kron): Remove usage of Module and use TaskQueue instead.
class PlaybackTimingProcessor : public Module, public PlaybackTimingCallback {
 public:
  PlaybackTimingProcessor(Clock* clock,
                          TransportFeedbackSenderInterface* feedback_sender);

  // Implement Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Methods for updating timing information.
  void UpdatePacketAndDecodeTiming(uint32_t ssrc,
                                   const TimingInfo& timing_info) override;

 protected:
  webrtc::SequenceChecker module_sequence_checker_;
  static constexpr TimeDelta kProcessInterval = TimeDelta::Millis(100);
  Clock* const clock_;
  TransportFeedbackSenderInterface* const feedback_sender_;
  Timestamp last_process_time_ RTC_GUARDED_BY(module_sequence_checker_);
  rtc::CriticalSection timing_infos_crit_;
  std::map<uint32_t, std::vector<TimingInfo>> timing_infos_
      RTC_GUARDED_BY(&timing_infos_crit_);
  uint32_t rtp_timestamp_last_decoded_ RTC_GUARDED_BY(&timing_infos_crit_);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_PLAYBACK_TIMING_PROCESSOR_PLAYBACK_TIMING_PROCESSOR_H_
