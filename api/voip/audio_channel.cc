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
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
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
  rtp_receive_statistics_ = ReceiveStatistics::Create(clock);
  ntp_estimator_ = std::make_unique<RemoteNtpTimeEstimator>(clock);

  RtpRtcp::Configuration rtp_config;
  rtp_config.clock = clock;
  rtp_config.audio = true;
  rtp_config.receive_statistics = rtp_receive_statistics_.get();
  rtp_config.rtcp_report_interval_ms = kRtcpReportIntervalMs;
  // TODO(natim): set these with config during channel creation
  rtp_config.outgoing_transport = this;
  rtp_config.local_media_ssrc = 0xDEADC0DE;

  rtp_rtcp_ = RtpRtcp::Create(rtp_config);
  rtp_rtcp_->SetSendingMediaStatus(false);
  // Ensure that RTCP is enabled for the created channel.
  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);

  // Process thread periodically services rtp stack for rtcp
  process_thread_->RegisterModule(rtp_rtcp_.get(), RTC_FROM_HERE);

  ingress_ = std::make_unique<AudioIngress>(decoder_factory);
  egress_ =
      std::make_unique<AudioEgress>(rtp_rtcp_.get(), clock, task_queue_factory);

  ingress_->SetNtpEstimator([this](uint32_t timestamp) {
    rtc::CritScope lock(&ntp_lock_);
    return ntp_estimator_->Estimate(timestamp);
  });

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

  RtpPacketReceived rtp_packet;
  rtp_packet.Parse(data, length);
  rtp_receive_statistics_->OnRtpPacket(rtp_packet);

  ingress_->ReceivedRTPPacket(&rtp_packet);
}

void AudioChannel::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // Deliver RTCP packet to RTP/RTCP module for parsing
  rtp_rtcp_->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return;
  }

  uint32_t ntp_secs = 0, ntp_frac = 0, rtp_timestamp = 0;
  if (rtp_rtcp_->RemoteNTP(&ntp_secs, &ntp_frac, nullptr, nullptr,
                           &rtp_timestamp) != 0) {
    // Waiting for RTCP.
    return;
  }

  {
    rtc::CritScope lock(&ntp_lock_);
    ntp_estimator_->UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
}

int64_t AudioChannel::GetRTT() const {
  std::vector<RTCPReportBlock> report_blocks;
  rtp_rtcp_->RemoteRTCPStat(&report_blocks);

  if (report_blocks.empty()) {
    return 0;
  }

  // TODO(natim): handle the case where remote end is changing ssrc and update
  // accordingly here.
  //
  // We don't know in advance the remote ssrc used by the other end's receiver
  // reports, so use the SSRC of the first report block as remote ssrc for now.
  uint32_t remote_ssrc = report_blocks[0].sender_ssrc;
  if (remote_ssrc != static_cast<uint32_t>(ingress_->Ssrc())) {
    rtp_rtcp_->SetRemoteSSRC(remote_ssrc);
    // AudioMixer::Source requires to provide remote SSRC thougt not used
    // in this voip use case yet.  We'll use the storage as to track the
    // current remote SSRC.
    ingress_->SetRemoteSSRC(remote_ssrc);
  }

  int64_t rtt = 0;
  int64_t avg_rtt = 0;
  int64_t max_rtt = 0;
  int64_t min_rtt = 0;

  if (rtp_rtcp_->RTT(remote_ssrc, &rtt, &avg_rtt, &min_rtt, &max_rtt) != 0) {
    return 0;
  }
  return rtt;
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
