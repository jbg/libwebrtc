//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#include "api/voip/audio_channel.h"

#include <utility>
#include <vector>

#include "api/audio_codecs/audio_format.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr int kRtcpReportIntervalMs = 5000;

}  // namespace

AudioChannel::AudioChannel(
    Clock* clock,
    TaskQueueFactory* task_queue_factory,
    ProcessThread* process_thread,
    AudioMixer* audio_mixer,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory)
    : audio_mixer_(audio_mixer), process_thread_(process_thread) {
  RTC_DCHECK(task_queue_factory);
  RTC_DCHECK(process_thread);
  RTC_DCHECK(audio_mixer);

  // Create RtpStack
  // RTP Receive statistics are created here as required for RtpRtcp
  // instance creation.  However, it's handed off to audio ingress for
  // actual usage while the scope for RtpRtcp instance is maintained here.
  auto receive_statistics = ReceiveStatistics::Create(clock);

  RtpRtcp::Configuration rtp_config;
  rtp_config.clock = clock;
  rtp_config.audio = true;
  rtp_config.receive_statistics = receive_statistics.get();
  rtp_config.rtcp_report_interval_ms = kRtcpReportIntervalMs;

  // TODO(natim): set these with config during channel creation
  //              local ssrc could be set by application via sub-API
  rtp_config.outgoing_transport = this;
  rtp_config.local_media_ssrc = 0xDEADC0DE;

  rtp_rtcp_ = RtpRtcp::Create(rtp_config);
  rtp_rtcp_->SetSendingMediaStatus(false);
  // Ensure that RTCP is enabled for the created channel.
  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);

  // Process thread periodically services rtp stack for rtcp
  process_thread_->RegisterModule(rtp_rtcp_.get(), RTC_FROM_HERE);

  ingress_ = std::make_unique<AudioIngress>(
      rtp_rtcp_.get(), clock, decoder_factory, std::move(receive_statistics));
  egress_ =
      std::make_unique<AudioEgress>(rtp_rtcp_.get(), clock, task_queue_factory);

  // Add this instance of audio ingress to be part of audio mixer for ADM to
  // fetch audio samples to play.
  audio_mixer_->AddSource(ingress_.get());
}

AudioChannel::~AudioChannel() {
  audio_mixer_->RemoveSource(ingress_.get());
  process_thread_->DeRegisterModule(rtp_rtcp_.get());
}

void AudioChannel::SetChannelId(int channel) {
  RTC_LOG(INFO) << "SetChannelId: " << channel;
  channel_ = channel;
}

AudioEgress* AudioChannel::GetAudioEgress() {
  return egress_.get();
}

AudioIngress* AudioChannel::GetAudioIngress() {
  return ingress_.get();
}

bool AudioChannel::SendRtp(const uint8_t* packet,
                           size_t length,
                           const PacketOptions& options) {
  rtc::CritScope scoped(&lock_);
  if (transport_) {
    return transport_->SendRtp(packet, length, options);
  }
  return false;
}

bool AudioChannel::SendRtcp(const uint8_t* packet, size_t length) {
  rtc::CritScope scoped(&lock_);
  if (transport_) {
    return transport_->SendRtcp(packet, length);
  }
  return false;
}

bool AudioChannel::RegisterTransport(Transport* transport) {
  rtc::CritScope scoped(&lock_);
  if (transport_) {
    RTC_LOG(WARNING) << "channel " << channel_ << ": transport already set";
    return false;
  }
  transport_ = transport;
  return true;
}

bool AudioChannel::DeRegisterTransport() {
  rtc::CritScope scoped(&lock_);
  if (!transport_) {
    RTC_LOG(WARNING) << "channel " << channel_ << ": transport is not set";
    return false;
  }
  transport_ = nullptr;
  return true;
}

void AudioChannel::ReceivedRTPPacket(const uint8_t* data, size_t length) {
  if (!ingress_->Playing()) {
    return;
  }

  ingress_->ReceivedRTPPacket(data, length);
}

void AudioChannel::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  ingress_->ReceivedRTCPPacket(data, length);
}

void AudioChannel::StartSend() {
  egress_->Start();

  if (!rtp_rtcp_->Sending()) {
    if (rtp_rtcp_->SetSendingStatus(true) != 0) {
      RTC_DLOG(LS_ERROR) << "StartSend() RTP/RTCP failed to start sending";
    }
  }
}

void AudioChannel::StopSend() {
  egress_->Stop();

  // Reset sending SSRC and sequence number and triggers direct transmission
  // of RTCP BYE
  if (!ingress_->Playing() && rtp_rtcp_->Sending()) {
    if (rtp_rtcp_->SetSendingStatus(false) == -1) {
      RTC_DLOG(LS_ERROR) << "StopSend() RTP/RTCP failed to stop sending";
    }
  }
}

void AudioChannel::StartPlay() {
  ingress_->Start();

  // Even in recv-only mode, RTCP receiver report is required to be sent back.
  if (!rtp_rtcp_->Sending()) {
    if (rtp_rtcp_->SetSendingStatus(true) != 0) {
      RTC_DLOG(LS_ERROR) << "StartPlay() RTP/RTCP failed to start sending";
    }
  }
}

void AudioChannel::StopPlay() {
  ingress_->Stop();

  // Reset sending SSRC and sequence number and triggers direct transmission
  // of RTCP BYE
  if (!egress_->Sending() && rtp_rtcp_->Sending()) {
    if (rtp_rtcp_->SetSendingStatus(false) != 0) {
      RTC_DLOG(LS_ERROR) << "StopPlay() RTP/RTCP failed to stop sending";
    }
  }
}

}  // namespace webrtc
