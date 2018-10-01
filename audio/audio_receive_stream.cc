/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_receive_stream.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "api/call/audio_sink.h"
#include "audio/audio_send_stream.h"
#include "audio/audio_state.h"
#include "audio/channel_proxy.h"
#include "audio/conversion.h"
#include "audio/rtp_audio_stream_receiver.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/rtp_stream_receiver_controller_interface.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

std::string AudioReceiveStream::Config::Rtp::ToString() const {
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", transport_cc: " << (transport_cc ? "on" : "off");
  ss << ", nack: " << nack.ToString();
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1) {
      ss << ", ";
    }
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string AudioReceiveStream::Config::ToString() const {
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss << "{rtp: " << rtp.ToString();
  ss << ", rtcp_send_transport: "
     << (rtcp_send_transport ? "(Transport)" : "null");
  if (!sync_group.empty()) {
    ss << ", sync_group: " << sync_group;
  }
  ss << '}';
  return ss.str();
}

namespace internal {
namespace {

constexpr double kAudioSampleDurationSeconds = 0.01;

std::unique_ptr<AudioCodingModule> CreateAudioCodingModule(
    const webrtc::AudioReceiveStream::Config& config) {
  AudioCodingModule::Config acm_config;
  acm_config.decoder_factory = config.decoder_factory;
  acm_config.neteq_config.codec_pair_id = config.codec_pair_id;
  acm_config.neteq_config.max_packets_in_buffer =
      config.jitter_buffer_max_packets;
  acm_config.neteq_config.enable_fast_accelerate =
      config.jitter_buffer_fast_accelerate;
  acm_config.neteq_config.enable_muted_state = true;
  return absl::WrapUnique(AudioCodingModule::Create(acm_config));
}

#if 0
std::unique_ptr<voe::ChannelProxy> CreateChannelAndProxy(
    webrtc::AudioState* audio_state,
    ProcessThread* module_process_thread,
    const webrtc::AudioReceiveStream::Config& config,
    RtcEventLog* event_log) {
  RTC_DCHECK(audio_state);
  internal::AudioState* internal_audio_state =
      static_cast<internal::AudioState*>(audio_state);
  return absl::make_unique<voe::ChannelProxy>(absl::make_unique<voe::Channel>(
      module_process_thread, internal_audio_state->audio_device_module(),
      nullptr /* RtcpRttStats */, event_log, config.rtp.remote_ssrc,
      config.jitter_buffer_max_packets, config.jitter_buffer_fast_accelerate,
      config.decoder_factory, config.codec_pair_id));
}
#endif
}  // namespace

AudioReceiveStream::AudioReceiveStream(
    RtpStreamReceiverControllerInterface* receiver_controller,
    PacketRouter* packet_router,
    const webrtc::AudioReceiveStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    webrtc::RtcEventLog* event_log)
    : audio_state_(audio_state),
      event_log_(event_log),
      audio_coding_(CreateAudioCodingModule(config)),
      rtp_audio_stream_receiver_(
          absl::make_unique<RtpAudioStreamReceiver>(packet_router,
                                                    config.rtp,
                                                    config.rtcp_send_transport,
                                                    audio_coding_.get(),
                                                    event_log)) {
  RTC_LOG(LS_INFO) << "AudioReceiveStream: " << config.rtp.remote_ssrc;
  RTC_DCHECK(receiver_controller);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(config.decoder_factory);
  RTC_DCHECK(audio_state_);

  module_process_thread_checker_.DetachFromThread();

  // Register with transport.
  rtp_stream_receiver_ = receiver_controller->CreateReceiver(
      config.rtp.remote_ssrc, rtp_audio_stream_receiver_.get());

  ConfigureStream(this, config, true);
}

AudioReceiveStream::~AudioReceiveStream() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "~AudioReceiveStream: " << config_.rtp.remote_ssrc;
  Stop();
}

void AudioReceiveStream::Reconfigure(
    const webrtc::AudioReceiveStream::Config& config) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  ConfigureStream(this, config, false);
}

void AudioReceiveStream::Start() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (playing_) {
    return;
  }
  rtp_audio_stream_receiver_->StartPlayout();
  playing_ = true;
  audio_state()->AddReceivingStream(this);
}

void AudioReceiveStream::Stop() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playing_) {
    return;
  }
  rtp_audio_stream_receiver_->StopPlayout();
  output_audio_level_.Clear();
  playing_ = false;
  audio_state()->RemoveReceivingStream(this);
}

webrtc::AudioReceiveStream::Stats AudioReceiveStream::GetStats() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  webrtc::AudioReceiveStream::Stats stats;
  stats.remote_ssrc = config_.rtp.remote_ssrc;
#if 0
  rtc::CritScope lock(&ts_stats_lock_);
#endif
  stats.capture_start_ntp_time_ms = capture_start_ntp_time_ms_;

  RtpAudioStreamReceiver::Stats call_stats =
      rtp_audio_stream_receiver_->GetRtpStatistics();

  // TODO(solenberg): Don't return here if we can't get the codec - return the
  //                  stats we *can* get.
  webrtc::CodecInst codec_inst = {0};
  if (audio_coding_->ReceiveCodec(&codec_inst) != 0) {
    return stats;
  }

  stats.bytes_rcvd = call_stats.bytesReceived;
  stats.packets_rcvd = call_stats.packetsReceived;
  stats.packets_lost = call_stats.cumulativeLost;
  stats.fraction_lost = Q8ToFloat(call_stats.fractionLost);

  if (codec_inst.pltype != -1) {
    stats.codec_name = codec_inst.plname;
    stats.codec_payload_type = codec_inst.pltype;
  }
  stats.ext_seqnum = call_stats.extendedMax;
  if (codec_inst.plfreq / 1000 > 0) {
    stats.jitter_ms = call_stats.jitterSamples / (codec_inst.plfreq / 1000);
  }

  // TODO(nisse): XXX Add PlayoutDelay from AudioDeviceModule
  stats.delay_estimate_ms = audio_coding_->FilteredCurrentDelayMs();

  stats.audio_level = output_audio_level_.LevelFullRange();
  stats.total_output_energy = output_audio_level_.TotalEnergy();
  stats.total_output_duration = output_audio_level_.TotalDuration();

  // Get jitter buffer and total delay (alg + jitter + playout) stats.
  NetworkStatistics ns = {0};
  int error = audio_coding_->GetNetworkStatistics(&ns);
  RTC_DCHECK_EQ(error, 0);

  stats.jitter_buffer_ms = ns.currentBufferSize;
  stats.jitter_buffer_preferred_ms = ns.preferredBufferSize;
  stats.total_samples_received = ns.totalSamplesReceived;
  stats.concealed_samples = ns.concealedSamples;
  stats.concealment_events = ns.concealmentEvents;
  stats.jitter_buffer_delay_seconds =
      static_cast<double>(ns.jitterBufferDelayMs) /
      static_cast<double>(rtc::kNumMillisecsPerSec);
  stats.expand_rate = Q14ToFloat(ns.currentExpandRate);
  stats.speech_expand_rate = Q14ToFloat(ns.currentSpeechExpandRate);
  stats.secondary_decoded_rate = Q14ToFloat(ns.currentSecondaryDecodedRate);
  stats.secondary_discarded_rate = Q14ToFloat(ns.currentSecondaryDiscardedRate);
  stats.accelerate_rate = Q14ToFloat(ns.currentAccelerateRate);
  stats.preemptive_expand_rate = Q14ToFloat(ns.currentPreemptiveRate);
  AudioDecodingCallStats ds;
  audio_coding_->GetDecodingCallStatistics(&ds);

  stats.decoding_calls_to_silence_generator = ds.calls_to_silence_generator;
  stats.decoding_calls_to_neteq = ds.calls_to_neteq;
  stats.decoding_normal = ds.decoded_normal;
  stats.decoding_plc = ds.decoded_plc;
  stats.decoding_cng = ds.decoded_cng;
  stats.decoding_plc_cng = ds.decoded_plc_cng;
  stats.decoding_muted_output = ds.decoded_muted_output;

  return stats;
}

void AudioReceiveStream::SetSink(AudioSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  raw_audio_sink_ = sink;
}

void AudioReceiveStream::SetGain(float gain) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  output_gain_ = gain;
}

std::vector<RtpSource> AudioReceiveStream::GetSources() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return rtp_audio_stream_receiver_->GetSources();
}

AudioMixer::Source::AudioFrameInfo AudioReceiveStream::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
#if 0
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
#endif
  audio_frame->sample_rate_hz_ = sample_rate_hz;

  event_log_->Log(absl::make_unique<RtcEventAudioPlayout>(Ssrc()));
  // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
  bool muted;
  if (audio_coding_->PlayoutData10Ms(audio_frame->sample_rate_hz_, audio_frame,
                                     &muted) == -1) {
    RTC_DLOG(LS_ERROR) << "Channel::GetAudioFrame() PlayoutData10Ms() failed!";
    // In all likelihood, the audio in this frame is garbage. We return an
    // error so that the audio mixer module doesn't add it to the mix. As
    // a result, it won't be played out and the actions skipped here are
    // irrelevant.
    return AudioMixer::Source::AudioFrameInfo::kError;
  }

  if (muted) {
    // TODO(henrik.lundin): We should be able to do better than this. But we
    // will have to go through all the cases below where the audio samples may
    // be used, and handle the muted case in some way.
    AudioFrameOperations::Mute(audio_frame);
  }

  {
// Pass the audio buffers to an optional sink callback, before applying
// scaling/panning, as that applies to the mix operation.
// External recipients of the audio (e.g. via AudioTrack), will do their
// own mixing/dynamic processing.
#if 0
    rtc::CritScope cs(&_callbackCritSect);
#endif
    if (raw_audio_sink_) {
      AudioSinkInterface::Data data(
          audio_frame->data(), audio_frame->samples_per_channel_,
          audio_frame->sample_rate_hz_, audio_frame->num_channels_,
          audio_frame->timestamp_);
      raw_audio_sink_->OnData(data);
    }
  }

  float output_gain = 1.0f;
  {
#if 0
    rtc::CritScope cs(&volume_settings_critsect_);
#endif
    output_gain = output_gain_;
  }

  // Output volume scaling
  if (output_gain < 0.99f || output_gain > 1.01f) {
    // TODO(solenberg): Combine with mute state - this can cause clicks!
    AudioFrameOperations::ScaleWithSat(output_gain, audio_frame);
  }

  // Measure audio level (0-9)
  // TODO(henrik.lundin) Use the |muted| information here too.
  // TODO(deadbeef): Use RmsLevel for |output_audio_level| (see
  // https://crbug.com/webrtc/7517).
  output_audio_level_.ComputeLevel(*audio_frame, kAudioSampleDurationSeconds);

  // TODO(nisse): Move this logic to RtpAudioStreamReceiver.
  if (capture_start_rtp_time_stamp_ < 0 && audio_frame->timestamp_ != 0) {
    // The first frame with a valid rtp timestamp.
    capture_start_rtp_time_stamp_ = audio_frame->timestamp_;
  }

  if (capture_start_rtp_time_stamp_ >= 0) {
    // audio_frame.timestamp_ should be valid from now on.

    // Compute elapsed time.
    int64_t unwrap_timestamp =
        rtp_ts_wraparound_handler_.Unwrap(audio_frame->timestamp_);

    const auto format = audio_coding_->ReceiveFormat();
    // Default to the playout frequency if we've not gotten any packets yet.
    // TODO(ossu): Zero clockrate can only happen if we've added an external
    // decoder for a format we don't support internally. Remove once that way of
    // adding decoders is gone!
    int frequency_hz = (format && format->clockrate_hz != 0)
                           ? format->clockrate_hz
                           : audio_coding_->PlayoutFrequency();

    audio_frame->elapsed_time_ms_ =
        (unwrap_timestamp - capture_start_rtp_time_stamp_) /
        (frequency_hz / 1000);

    {
#if 0
      rtc::CritScope lock(&ts_stats_lock_);
#endif
      // Compute ntp time.
      audio_frame->ntp_time_ms_ =
          rtp_audio_stream_receiver_->EstimateNtpMs(audio_frame->timestamp_);

      // |ntp_time_ms_| won't be valid until at least 2 RTCP SRs are received.
      if (audio_frame->ntp_time_ms_ > 0) {
        // Compute |capture_start_ntp_time_ms_| so that
        // |capture_start_ntp_time_ms_| + |elapsed_time_ms_| == |ntp_time_ms_|
        capture_start_ntp_time_ms_ =
            audio_frame->ntp_time_ms_ - audio_frame->elapsed_time_ms_;
      }
    }
  }

  {
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.TargetJitterBufferDelayMs",
                              audio_coding_->TargetDelayMs());
    const int jitter_buffer_delay = audio_coding_->FilteredCurrentDelayMs();
#if 0
    rtc::CritScope lock(&video_sync_lock_);
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDelayEstimateMs",
                              jitter_buffer_delay + playout_delay_ms_);
#endif
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverJitterBufferDelayMs",
                              jitter_buffer_delay);
#if 0
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDeviceDelayMs",
                              playout_delay_ms_);
#endif
  }

  return muted ? AudioMixer::Source::AudioFrameInfo::kMuted
               : AudioMixer::Source::AudioFrameInfo::kNormal;
}

int AudioReceiveStream::Ssrc() const {
  return config_.rtp.remote_ssrc;
}

int AudioReceiveStream::PreferredSampleRate() const {
  // Return the bigger of playout and receive frequency in the ACM.
  return std::max(audio_coding_->ReceiveFrequency(),
                  audio_coding_->PlayoutFrequency());
}

int AudioReceiveStream::id() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return config_.rtp.remote_ssrc;
}

absl::optional<Syncable::Info> AudioReceiveStream::GetInfo() const {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  absl::optional<Syncable::Info> info =
      rtp_audio_stream_receiver_->GetSyncInfo();

  if (!info)
    return absl::nullopt;

  // TODO(nisse): XXX Add PlayoutDelay from AudioDeviceModule
  info->current_delay_ms = audio_coding_->FilteredCurrentDelayMs();
  return info;
}

uint32_t AudioReceiveStream::GetPlayoutTimestamp() const {
  uint32_t playout_timestamp_rtp =
      rtp_audio_stream_receiver_->GetRtpTimestamp().value_or(0);
  uint16_t delay_ms = 0;
  // TODO(nisse): What's the right way to get access to the audio device module?
  if (static_cast<internal::AudioState*>(audio_state_.get())
          ->audio_device_module()
          ->PlayoutDelay(&delay_ms) == 0) {
    absl::optional<SdpAudioFormat> format = audio_coding_->ReceiveFormat();
    // Default to the playout frequency if we've not gotten any packets yet.
    // TODO(ossu): Zero clockrate can only happen if we've added an external
    // decoder for a format we don't support internally. Remove once that way of
    // adding decoders is gone!
    int frequency_hz = (format && format->clockrate_hz != 0)
                           ? format->clockrate_hz
                           : audio_coding_->PlayoutFrequency();

    playout_timestamp_rtp -= delay_ms * (frequency_hz / 1000);
  }
  return playout_timestamp_rtp;
}

void AudioReceiveStream::SetMinimumPlayoutDelay(int delay_ms) {
  constexpr int kVoiceEngineMinMinPlayoutDelayMs = 0;
  constexpr int kVoiceEngineMaxMinPlayoutDelayMs = 10000;
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);

  if ((delay_ms < kVoiceEngineMinMinPlayoutDelayMs) ||
      (delay_ms > kVoiceEngineMaxMinPlayoutDelayMs)) {
    RTC_DLOG(LS_ERROR) << "SetMinimumPlayoutDelay() invalid min delay";
    return;
  }
  if (audio_coding_->SetMinimumPlayoutDelay(delay_ms) != 0) {
    RTC_DLOG(LS_ERROR)
        << "SetMinimumPlayoutDelay() failed to set min playout delay";
    return;
  }
}

void AudioReceiveStream::AssociateSendStream(AudioSendStream* send_stream) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (send_stream) {
    rtp_audio_stream_receiver_->AssociateSendChannel(
        &send_stream->GetChannelProxy());
  } else {
    rtp_audio_stream_receiver_->DisassociateSendChannel();
  }
  associated_send_stream_ = send_stream;
}

void AudioReceiveStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
}

bool AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  rtp_audio_stream_receiver_->OnRtcpPacket(rtc::MakeArrayView(packet, length));
  return true;
}

// TODO(nisse): Used by tests only. Delete.
void AudioReceiveStream::OnRtpPacket(const RtpPacketReceived& packet) {
  rtp_audio_stream_receiver_->OnRtpPacket(packet);
}

const webrtc::AudioReceiveStream::Config& AudioReceiveStream::config() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return config_;
}

const AudioSendStream* AudioReceiveStream::GetAssociatedSendStreamForTesting()
    const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return associated_send_stream_;
}

internal::AudioState* AudioReceiveStream::audio_state() const {
  auto* audio_state = static_cast<internal::AudioState*>(audio_state_.get());
  RTC_DCHECK(audio_state);
  return audio_state;
}

// static
void AudioReceiveStream::ConfigureStream(AudioReceiveStream* stream,
                                         const Config& new_config,
                                         bool first_time) {
  RTC_LOG(LS_INFO) << "AudioReceiveStream::ConfigureStream: "
                   << new_config.ToString();
  RTC_DCHECK(stream);
  const auto& old_config = stream->config_;

  // Configuration parameters which cannot be changed.
  RTC_DCHECK(first_time ||
             old_config.rtp.remote_ssrc == new_config.rtp.remote_ssrc);
  RTC_DCHECK(first_time ||
             old_config.rtcp_send_transport == new_config.rtcp_send_transport);
  // Decoder factory cannot be changed because it is configured at
  // voe::Channel construction time.
  RTC_DCHECK(first_time ||
             old_config.decoder_factory == new_config.decoder_factory);

  if (first_time || old_config.rtp.local_ssrc != new_config.rtp.local_ssrc) {
    stream->rtp_audio_stream_receiver_->SetLocalSSRC(new_config.rtp.local_ssrc);
  }

  if (!first_time) {
    // Remote ssrc can't be changed mid-stream.
    RTC_DCHECK_EQ(old_config.rtp.remote_ssrc, new_config.rtp.remote_ssrc);
  }

  // TODO(solenberg): Config NACK history window (which is a packet count),
  // using the actual packet size for the configured codec.
  if (first_time || old_config.rtp.nack.rtp_history_ms !=
                        new_config.rtp.nack.rtp_history_ms) {
    int max_number_of_packets = new_config.rtp.nack.rtp_history_ms / 20;
    bool enable = (max_number_of_packets > 0);
    stream->rtp_audio_stream_receiver_->SetNACKStatus(max_number_of_packets);
    if (enable)
      stream->audio_coding_->EnableNack(max_number_of_packets);
    else
      stream->audio_coding_->DisableNack();
  }
  if (first_time || old_config.decoder_map != new_config.decoder_map) {
    std::map<uint8_t, int> payload_type_frequencies;
    for (const auto& kv : new_config.decoder_map) {
      RTC_DCHECK_GE(kv.second.clockrate_hz, 1000);
      payload_type_frequencies[kv.first] = kv.second.clockrate_hz;
    }
    stream->rtp_audio_stream_receiver_->SetPayloadTypeFrequencies(
        std::move(payload_type_frequencies));
    stream->audio_coding_->SetReceiveCodecs(new_config.decoder_map);
  }

  stream->config_ = new_config;
}
}  // namespace internal
}  // namespace webrtc
