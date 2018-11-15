/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_CHANNEL_RECEIVE_H_
#define AUDIO_CHANNEL_RECEIVE_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/crypto/cryptooptions.h"
#include "api/media_transport_interface.h"
#include "api/rtpreceiverinterface.h"
#include "audio/audio_level.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/contributing_sources.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/thread_checker.h"

// TODO(solenberg, nisse): This file contains a few NOLINT marks, to silence
// warnings about use of unsigned short, and non-const reference arguments.
// These need cleanup, in a separate cl.

namespace rtc {
class TimestampWrapAroundHandler;
}

namespace webrtc {

class AudioDeviceModule;
class FrameDecryptorInterface;
class PacketRouter;
class ProcessThread;
class RateLimiter;
class ReceiveStatistics;
class RtcEventLog;
class RtpPacketReceived;
class RtpRtcp;

struct CallReceiveStatistics {
  unsigned short fractionLost;  // NOLINT
  unsigned int cumulativeLost;
  unsigned int extendedMax;
  unsigned int jitterSamples;
  int64_t rttMs;
  size_t bytesReceived;
  int packetsReceived;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_;
};

namespace voe {

class ChannelSend;

// Interface class needed for AudioReceiveStream tests that use a
// MockChannelReceive.

class ChannelReceiveInterface : public RtpPacketSinkInterface {
 public:
  virtual ~ChannelReceiveInterface() = default;

  virtual void SetSink(AudioSinkInterface* sink) = 0;

  virtual void SetReceiveCodecs(
      const std::map<int, SdpAudioFormat>& codecs) = 0;

  virtual void StartPlayout() = 0;
  virtual void StopPlayout() = 0;

  virtual bool GetRecCodec(CodecInst* codec) const = 0;

  virtual bool ReceivedRTCPPacket(const uint8_t* data, size_t length) = 0;

  virtual void SetChannelOutputVolumeScaling(float scaling) = 0;
  virtual int GetSpeechOutputLevelFullRange() const = 0;
  // See description of "totalAudioEnergy" in the WebRTC stats spec:
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
  virtual double GetTotalOutputEnergy() const = 0;
  virtual double GetTotalOutputDuration() const = 0;

  // Stats.
  virtual NetworkStatistics GetNetworkStatistics() const = 0;
  virtual AudioDecodingCallStats GetDecodingCallStatistics() const = 0;

  // Audio+Video Sync.
  virtual uint32_t GetDelayEstimate() const = 0;
  virtual void SetMinimumPlayoutDelay(int delay_ms) = 0;
  virtual uint32_t GetPlayoutTimestamp() const = 0;

  // Produces the transport-related timestamps; current_delay_ms is left unset.
  virtual absl::optional<Syncable::Info> GetSyncInfo() const = 0;

  // RTP+RTCP
  virtual void SetLocalSSRC(uint32_t ssrc) = 0;

  virtual void RegisterReceiverCongestionControlObjects(
      PacketRouter* packet_router) = 0;
  virtual void ResetReceiverCongestionControlObjects() = 0;

  virtual CallReceiveStatistics GetRTCPStatistics() const = 0;
  virtual void SetNACKStatus(bool enable, int max_packet) = 0;

  // From AudioMixer::Source.
  virtual AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame) = 0;

  virtual int PreferredSampleRate() const = 0;

  // Associate to a send channel.
  // Used for obtaining RTT for a receive-only channel.
  virtual void SetAssociatedSendChannel(const ChannelSend* channel) = 0;

  virtual std::vector<RtpSource> GetSources() const = 0;
};

// Helper class to simplify locking scheme for members that are accessed from
// multiple threads.
// Example: a member can be set on thread T1 and read by an internal audio
// thread T2. Accessing the member via this class ensures that we are
// safe and also avoid TSan v2 warnings.
class ChannelReceiveState {
 public:
  struct State {
    bool playing = false;
  };

  ChannelReceiveState() {}
  virtual ~ChannelReceiveState() {}

  void Reset() {
    rtc::CritScope lock(&lock_);
    state_ = State();
  }

  State Get() const {
    rtc::CritScope lock(&lock_);
    return state_;
  }

  void SetPlaying(bool enable) {
    rtc::CritScope lock(&lock_);
    state_.playing = enable;
  }

 private:
  rtc::CriticalSection lock_;
  State state_;
};

class ChannelReceive : public ChannelReceiveInterface,
                       public MediaTransportAudioSinkInterface {
 public:
  // Used for receive streams.
  ChannelReceive(ProcessThread* module_process_thread,
                 AudioDeviceModule* audio_device_module,
                 MediaTransportInterface* media_transport,
                 Transport* rtcp_send_transport,
                 RtcEventLog* rtc_event_log,
                 uint32_t remote_ssrc,
                 size_t jitter_buffer_max_packets,
                 bool jitter_buffer_fast_playout,
                 rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
                 absl::optional<AudioCodecPairId> codec_pair_id,
                 rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
                 const webrtc::CryptoOptions& crypto_options);
  ~ChannelReceive() override;

  void SetSink(AudioSinkInterface* sink) override;

  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs) override;

  // API methods

  void StartPlayout() override;
  void StopPlayout() override;

  // Codecs
  bool GetRecCodec(CodecInst* codec) const override;

  bool ReceivedRTCPPacket(const uint8_t* data, size_t length) override;

  // RtpPacketSinkInterface.
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  // Muting, Volume and Level.
  void SetChannelOutputVolumeScaling(float scaling) override;
  int GetSpeechOutputLevelFullRange() const override;
  // See description of "totalAudioEnergy" in the WebRTC stats spec:
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
  double GetTotalOutputEnergy() const override;
  double GetTotalOutputDuration() const override;

  // Stats.
  NetworkStatistics GetNetworkStatistics() const override;
  AudioDecodingCallStats GetDecodingCallStatistics() const override;

  // Audio+Video Sync.
  uint32_t GetDelayEstimate() const override;
  void SetMinimumPlayoutDelay(int delayMs) override;
  uint32_t GetPlayoutTimestamp() const override;

  // Produces the transport-related timestamps; current_delay_ms is left unset.
  absl::optional<Syncable::Info> GetSyncInfo() const override;

  // RTP+RTCP
  void SetLocalSSRC(unsigned int ssrc) override;

  void RegisterReceiverCongestionControlObjects(
      PacketRouter* packet_router) override;
  void ResetReceiverCongestionControlObjects() override;

  CallReceiveStatistics GetRTCPStatistics() const override;
  void SetNACKStatus(bool enable, int maxNumberOfPackets) override;

  // From AudioMixer::Source.
  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame) override;

  int PreferredSampleRate() const override;

  // Associate to a send channel.
  // Used for obtaining RTT for a receive-only channel.
  void SetAssociatedSendChannel(const ChannelSend* channel) override;

  std::vector<RtpSource> GetSources() const override;

 private:
  void Init();
  void Terminate();

  int GetRemoteSSRC(unsigned int& ssrc);  // NOLINT

  bool ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header);
  int ResendPackets(const uint16_t* sequence_numbers, int length);
  void UpdatePlayoutTimestamp(bool rtcp);

  int GetRtpTimestampRateHz() const;
  int64_t GetRTT() const;

  // MediaTransportAudioSinkInterface override;
  void OnData(uint64_t channel_id,
              MediaTransportEncodedAudioFrame frame) override;

  int32_t OnReceivedPayloadData(const uint8_t* payloadData,
                                size_t payloadSize,
                                const WebRtcRTPHeader* rtpHeader);

  // Thread checkers document and lock usage of some methods to specific threads
  // we know about. The goal is to eventually split up voe::ChannelReceive into
  // parts with single-threaded semantics, and thereby reduce the need for
  // locks.
  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker module_process_thread_checker_;
  // Methods accessed from audio and video threads are checked for sequential-
  // only access. We don't necessarily own and control these threads, so thread
  // checkers cannot be used. E.g. Chromium may transfer "ownership" from one
  // audio thread to another, but access is still sequential.
  rtc::RaceChecker audio_thread_race_checker_;
  rtc::RaceChecker video_capture_thread_race_checker_;
  rtc::CriticalSection _callbackCritSect;
  rtc::CriticalSection volume_settings_critsect_;

  ChannelReceiveState channel_state_;

  RtcEventLog* const event_log_;

  // Indexed by payload type.
  std::map<uint8_t, int> payload_type_frequencies_;

  std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<RtpRtcp> _rtpRtcpModule;
  const uint32_t remote_ssrc_;

  // Info for GetSources and GetSyncInfo is updated on network or worker thread,
  // queried on the worker thread.
  rtc::CriticalSection rtp_sources_lock_;
  ContributingSources contributing_sources_ RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<int64_t> last_received_rtp_system_time_ms_
      RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<uint8_t> last_received_rtp_audio_level_
      RTC_GUARDED_BY(&rtp_sources_lock_);

  std::unique_ptr<AudioCodingModule> audio_coding_;
  AudioSinkInterface* audio_sink_ = nullptr;
  AudioLevel _outputAudioLevel;

  RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(ts_stats_lock_);

  // Timestamp of the audio pulled from NetEq.
  absl::optional<uint32_t> jitter_buffer_playout_timestamp_;

  rtc::CriticalSection video_sync_lock_;
  uint32_t playout_timestamp_rtp_ RTC_GUARDED_BY(video_sync_lock_);
  uint32_t playout_delay_ms_ RTC_GUARDED_BY(video_sync_lock_);

  rtc::CriticalSection ts_stats_lock_;

  std::unique_ptr<rtc::TimestampWrapAroundHandler> rtp_ts_wraparound_handler_;
  // The rtp timestamp of the first played out audio frame.
  int64_t capture_start_rtp_time_stamp_;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_ RTC_GUARDED_BY(ts_stats_lock_);

  // uses
  ProcessThread* _moduleProcessThreadPtr;
  AudioDeviceModule* _audioDeviceModulePtr;
  float _outputGain RTC_GUARDED_BY(volume_settings_critsect_);

  // An associated send channel.
  rtc::CriticalSection assoc_send_channel_lock_;
  const ChannelSend* associated_send_channel_
      RTC_GUARDED_BY(assoc_send_channel_lock_);

  PacketRouter* packet_router_ = nullptr;

  rtc::ThreadChecker construction_thread_;

  MediaTransportInterface* const media_transport_;

  // E2EE Audio Frame Decryption
  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_;
  webrtc::CryptoOptions crypto_options_;
};

}  // namespace voe
}  // namespace webrtc

#endif  // AUDIO_CHANNEL_RECEIVE_H_
