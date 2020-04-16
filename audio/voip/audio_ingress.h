/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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
#include "rtc_base/time_utils.h"

namespace webrtc {

// AudioIngress handles incoming RTP/RTCP packets from the remote
// media endpoint. Received RTP packets are injected into AcmReceiver and
// when audio output thread requests for audio samples to play through system
// output such as speaker device, AudioIngress provides the samples via its
// implementation on AudioMixer::Source interface.
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

  // Retreieve highest speech output level in last 100 ms.  Note that
  // this isn't RMS but absolute raw audio level on int16_t sample unit.
  // Therefore, the return value will vary between 0 ~ 0xFFFF. This type of
  // value may be useful to be used for measuring active speaker gauge.
  int GetSpeechOutputLevelFullRange() const;

  // Return network round trip (RTT) time measued by RTCP exchange with
  // remote media endpoint. RTT value 0 indicates that it's not initialized.
  int64_t GetRoundTripTime();

  NetworkStatistics GetNetworkStatistics() const;
  AudioDecodingCallStats GetDecodingStatistics() const;

  // Implementation of AudioMixer::Source interface.
  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sampling_rate,
      AudioFrame* audio_frame) override;
  int Ssrc() const override;
  int PreferredSampleRate() const override;

 private:
  // Internal class to provide thread safety around RemoteNtpTimeEstimator.
  // Methods defined here have same signature as underlying ones provided by
  // RemoteNtpTimeEstimator.
  class NtpEstimator {
   public:
    explicit NtpEstimator(Clock* clock);
    bool UpdateRtcpTimestamp(int64_t rtt,
                             uint32_t ntp_secs,
                             uint32_t ntp_frac,
                             uint32_t rtp_timestamp);
    int64_t Estimate(uint32_t rtp_timestamp);

   private:
    rtc::CriticalSection lock_;
    RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(lock_);
  };

  // For receiving RTP statistics, this tracks the sampling rate value
  // per payload type set when caller set via SetReceiveCodecs.
  class ReceiveCodecInfo {
   public:
    // Set receive codec info here as caller invokes SetReceiveCodecs.
    void SetCodecs(const std::map<int, SdpAudioFormat>& codecs);
    // Get corresponding sampling per given payload type.
    // Return -1 if it doesn't exist.
    int GetSamplingRate(int payload_type);

   private:
    rtc::CriticalSection lock_;
    std::map<int, int> payload_type_sampling_rate_ RTC_GUARDED_BY(lock_);
  };

  // Indicate AudioIngress status as caller invokes Start/StopPlaying.
  // If not playing, incoming RTP data processing is skipped, thus
  // producing no data to output device.
  std::atomic<bool> playing_;

  // Currently active remote ssrc from remote media endpoint.
  std::atomic<uint32_t> remote_ssrc_;

  // Synchronizaton is handled internally by ReceiveStatistics.
  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;

  // The first rtp timestamp of the output audio frame that is used to
  // calculate elasped time for subsequent audio frames. This only used within
  // GetAudioFrameWithInfo method which is typically accessed by single audio
  // thread. However, it has no side effect even with multiple threads entering
  // GetAudioFrameWithInfo.
  int64_t first_rtp_timestamp_;

  // This is only used within GetAudioFrameWithInfo method which is typically
  // accessed by single audio thread. If for some reason multiple threads enters
  // GetAudioFrameWithInfo, this may rarely result in inaccurate elapsed time
  // calculation which doesn't have severe side effect.
  rtc::TimestampWrapAroundHandler rtp_ts_wraparound_handler_;

  // Synchronizaton is handled internally by RtpRtcp.
  RtpRtcp* const rtp_rtcp_;

  // Synchronizaton is handled internally by acm2::AcmReceiver.
  acm2::AcmReceiver acm_receiver_;

  // Synchronizaton is handled internally by voe::AudioLevel.
  voe::AudioLevel output_audio_level_;

  NtpEstimator ntp_estimator_;
  ReceiveCodecInfo receive_codec_info_;
};

}  // namespace webrtc

#endif  // AUDIO_VOIP_AUDIO_INGRESS_H_
