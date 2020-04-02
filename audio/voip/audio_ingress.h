//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef AUDIO_VOIP_AUDIO_INGRESS_H_
#define AUDIO_VOIP_AUDIO_INGRESS_H_

#include <atomic>
#include <map>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/audio/audio_mixer.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "audio/audio_level.h"
#include "modules/audio_coding/acm2/acm_receiver.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

// AudioIngress handles incoming RTP/RTCP packets from the remote
// media endpoint.  Received RTP packets are injected into AcmReceiver and
// when audio output thread requests for audio samples to play through system
// output such as speaker device, AudioIngress provides the samples via its
// implementation on AudioMixer::Source interface.
//
// This class assumes single thread access on most APIs by caller which is
// enforced by SequenceChecker in debug mode except ReceivedRTPPacket and
// ReceivedRTCPPacket APIs which are network related APIs where caller may use
// a different thread to inject received RTP/RTCP packets.
//
// Note that this class is originally based on ChannelReceive in
// audio/channel_receive.cc with non-audio related logic trimmed as aimed for
// smaller footprint.
class AudioIngress : public AudioMixer::Source {
 public:
  AudioIngress(RtpRtcp* rtp_rtcp,
               Clock* clock,
               rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
               std::unique_ptr<ReceiveStatistics> receive_statistics);
  ~AudioIngress() override;

  // Start or stop receiving operation of AudioIngress.
  void StartPlay();
  void StopPlay();

  // Query the state of the AudioIngress.
  bool Playing() const;

  // Set the decoder formats and payload type for AcmReceiver where the
  // key type (int) of the map is the payload type of SdpAudioFormat.
  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs);

  // APIs to handle received RTP/RTCP packets from caller.
  void ReceivedRTPPacket(const uint8_t* data, size_t length);
  void ReceivedRTCPPacket(const uint8_t* data, size_t length);

  // Statistics APIs to be utilized in the future.
  int GetSpeechOutputLevelFullRange() const;
  int64_t GetRTT();
  NetworkStatistics GetNetworkStatistics() const;
  AudioDecodingCallStats GetDecodingCallStatistics() const;

  // Implementation of AudioMixer::Source interface.
  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sampling_rate,
      AudioFrame* audio_frame) override;
  int Ssrc() const override;
  int PreferredSampleRate() const override;

 private:
  SequenceChecker worker_thread_checker_;
  SequenceChecker network_thread_checker_;
  SequenceChecker audio_thread_checker_;

  // Indicate AudioIngress status as caller invokes Start/StopPlaying.
  // If not playing, incoming RTP data processing is skipped, thus
  // producing no data to output device.
  std::atomic<bool> playing_;

  // Currently active remote ssrc from remote media endpoint.
  std::atomic<uint32_t> remote_ssrc_;

  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_
      RTC_GUARDED_BY(network_thread_checker_);

  // The first rtp timestamp of the output audio frame that is used to
  // calculate elasped time for subsequent audio frames.
  int64_t first_rtp_timestamp_ RTC_GUARDED_BY(audio_thread_checker_);

  rtc::TimestampWrapAroundHandler rtp_ts_wraparound_handler_
      RTC_GUARDED_BY(audio_thread_checker_);

  // Synchronizaton is handled internally by RtpRtcp.
  RtpRtcp* const rtp_rtcp_;

  // Synchronizaton is handled internally by acm2::AcmReceiver.
  acm2::AcmReceiver acm_receiver_;

  // Synchronizaton is handled internally by voe::AudioLevel.
  voe::AudioLevel output_audio_level_;

  rtc::CriticalSection ntp_lock_;
  RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(ntp_lock_);

  // For receiving RTP statistics, this tracks the sampling rate value
  // per payload type set when caller set via SetReceiveCodecs.
  rtc::CriticalSection payload_type_lock_;
  std::map<int, int> payload_type_sampling_rate_
      RTC_GUARDED_BY(payload_type_lock_);
};

}  // namespace webrtc

#endif  // AUDIO_VOIP_AUDIO_INGRESS_H_
