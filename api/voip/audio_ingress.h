//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_AUDIO_INGRESS_H_
#define API_VOIP_AUDIO_INGRESS_H_

#include <map>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/audio/audio_mixer.h"
#include "api/call/transport.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "audio/audio_level.h"
#include "modules/audio_coding/acm2/acm_receiver.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

class AudioIngress : public AudioMixer::Source {
 public:
  explicit AudioIngress(
      rtc::scoped_refptr<AudioDecoderFactory> decoder_factory);
  ~AudioIngress() override;

  void Start();
  void Stop();
  bool Playing() const;

  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs);
  void SetNtpEstimator(std::function<int64_t(uint32_t)> estimate_ntp);
  void SetRemoteSSRC(uint32_t remote_ssrc);

  void ReceivedRTPPacket(RtpPacketReceived* rtp_packet);

  int GetSpeechOutputLevelFullRange() const;

  NetworkStatistics GetNetworkStatistics() const;
  AudioDecodingCallStats GetDecodingCallStatistics() const;

  // AudioMixer::Source
  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame) override;
  int Ssrc() const override;  // this is remote SSRC
  int PreferredSampleRate() const override;

 private:
  int GetRtpTimestampRateHz() const;

  rtc::ThreadChecker worker_thread_checker_;

  bool playing_ = false;
  // The rtp timestamp of the first played out audio frame.
  int64_t capture_start_rtp_time_stamp_ = -1;
  uint32_t remote_ssrc_ = 0;

  // Indexed by payload type.
  rtc::CriticalSection payload_type_lock_;
  std::map<uint8_t, int> payload_type_frequencies_
      RTC_GUARDED_BY(payload_type_lock_);

  std::unique_ptr<acm2::AcmReceiver> acm_receiver_;
  voe::AudioLevel output_audio_level_;
  std::function<int64_t(uint32_t)> estimate_ntp_;
  std::unique_ptr<rtc::TimestampWrapAroundHandler> rtp_ts_wraparound_handler_;
};

}  // namespace webrtc

#endif  // API_VOIP_AUDIO_INGRESS_H_
