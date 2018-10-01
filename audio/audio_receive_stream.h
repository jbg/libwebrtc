/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_RECEIVE_STREAM_H_
#define AUDIO_AUDIO_RECEIVE_STREAM_H_

#include <memory>
#include <vector>

#include "api/audio/audio_mixer.h"
#include "api/rtp_headers.h"
#include "audio/audio_state.h"
#include "call/audio_receive_stream.h"
#include "call/syncable.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
class AudioCodingModule;
class PacketRouter;
class ProcessThread;
class RtpAudioStreamReceiver;
class RtcEventLog;
class RtpPacketReceived;
class RtpStreamReceiverControllerInterface;
class RtpStreamReceiverInterface;

namespace internal {
class AudioSendStream;

class AudioReceiveStream final : public webrtc::AudioReceiveStream,
                                 public AudioMixer::Source,
                                 public Syncable {
 public:
  AudioReceiveStream(RtpStreamReceiverControllerInterface* receiver_controller,
                     PacketRouter* packet_router,
                     const webrtc::AudioReceiveStream::Config& config,
                     const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                     webrtc::RtcEventLog* event_log);
#if 0
  // For unit tests, which need to supply a mock channel proxy.
  AudioReceiveStream(RtpStreamReceiverControllerInterface* receiver_controller,
                     PacketRouter* packet_router,
                     const webrtc::AudioReceiveStream::Config& config,
                     const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                     webrtc::RtcEventLog* event_log,
                     std::unique_ptr<voe::ChannelProxy> channel_proxy);
#endif
  ~AudioReceiveStream() override;

  // webrtc::AudioReceiveStream implementation.
  void Reconfigure(const webrtc::AudioReceiveStream::Config& config) override;
  void Start() override;
  void Stop() override;
  webrtc::AudioReceiveStream::Stats GetStats() const override;
  void SetSink(AudioSinkInterface* sink) override;
  void SetGain(float gain) override;
  std::vector<webrtc::RtpSource> GetSources() const override;

  // TODO(nisse): We don't formally implement RtpPacketSinkInterface, and this
  // method shouldn't be needed. But it's currently used by the
  // AudioReceiveStreamTest.ReceiveRtpPacket unittest. Figure out if that test
  // shuld be refactored or deleted, and then delete this method.
  void OnRtpPacket(const RtpPacketReceived& packet);

  // AudioMixer::Source
  AudioFrameInfo GetAudioFrameWithInfo(int sample_rate_hz,
                                       AudioFrame* audio_frame) override;
  int Ssrc() const override;
  int PreferredSampleRate() const override;

  // Syncable
  int id() const override;
  absl::optional<Syncable::Info> GetInfo() const override;
  uint32_t GetPlayoutTimestamp() const override;
  void SetMinimumPlayoutDelay(int delay_ms) override;

  void AssociateSendStream(AudioSendStream* send_stream);
  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);
  const webrtc::AudioReceiveStream::Config& config() const;
  const AudioSendStream* GetAssociatedSendStreamForTesting() const;

 private:
  static void ConfigureStream(AudioReceiveStream* stream,
                              const Config& new_config,
                              bool first_time);

  AudioState* audio_state() const;

  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker module_process_thread_checker_;
  webrtc::AudioReceiveStream::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  RtcEventLog* const event_log_;
  const std::unique_ptr<AudioCodingModule> audio_coding_;
  const std::unique_ptr<webrtc::RtpAudioStreamReceiver>
      rtp_audio_stream_receiver_;
  AudioSendStream* associated_send_stream_ = nullptr;
  rtc::TimestampWrapAroundHandler rtp_ts_wraparound_handler_;
  // The rtp timestamp of the first played out audio frame.
  int64_t capture_start_rtp_time_stamp_;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_;  // XXX RTC_GUARDED_BY(ts_stats_lock_);

  AudioSinkInterface* raw_audio_sink_ = nullptr;

  // TODO(nisse): voe::Channel had this. Still needed?
  // rtc::CriticalSection volume_settings_critsect_;

  float output_gain_ = 1.0;  // XXX RTC_GUARDED_BY(volume_settings_critsect_);
  voe::AudioLevel output_audio_level_;

  bool playing_ RTC_GUARDED_BY(worker_thread_checker_) = false;

  std::unique_ptr<RtpStreamReceiverInterface> rtp_stream_receiver_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioReceiveStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // AUDIO_AUDIO_RECEIVE_STREAM_H_
