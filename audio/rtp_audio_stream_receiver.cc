/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/rtp_audio_stream_receiver.h"

#include "audio/channel_proxy.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"

namespace webrtc {

RtpAudioStreamReceiver::RtpAudioStreamReceiver(
    PacketRouter* packet_router,
    AudioReceiveStream::Config::Rtp rtp_config,
    Transport* rtcp_send_transport,
    AudioCodingModule* audio_coding,
    RtcEventLog* rtc_event_log)
    : packet_router_(packet_router),
      audio_coding_(audio_coding),
      receive_statistics_(ReceiveStatistics::Create(Clock::GetRealTimeClock())),
      ntp_estimator_(Clock::GetRealTimeClock()),
      remote_ssrc_(rtp_config.remote_ssrc) {
  receive_statistics_->EnableRetransmitDetection(remote_ssrc_, true);

  RtpRtcp::Configuration configuration;
  configuration.audio = true;
  configuration.receiver_only = true;
  // TODO(nisse): voe::Channel wraps the send transport and adds synchronization
  // via _callbackCritSect. Do we need something similar?
  configuration.outgoing_transport = rtcp_send_transport;
  configuration.receive_statistics = receive_statistics_.get();
  configuration.event_log = rtc_event_log;

  rtp_rtcp_ = absl::WrapUnique(RtpRtcp::CreateRtpRtcp(configuration));
  rtp_rtcp_->SetRemoteSSRC(remote_ssrc_);
  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);

  constexpr bool remb_candidate = false;
  packet_router_->AddReceiveRtpModule(rtp_rtcp_.get(), remb_candidate);
}

RtpAudioStreamReceiver::~RtpAudioStreamReceiver() {
  packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
}

void RtpAudioStreamReceiver::SetPayloadTypeFrequencies(
    std::map<uint8_t, int> payload_type_frequencies) {
  payload_type_frequencies_ = std::move(payload_type_frequencies);
}

void RtpAudioStreamReceiver::SetNACKStatus(int maxNumberOfPackets) {
  receive_statistics_->SetMaxReorderingThreshold(maxNumberOfPackets);
}

// TODO(nisse): Move to construction time, and delete this method.
void RtpAudioStreamReceiver::SetLocalSSRC(unsigned int ssrc) {
  rtp_rtcp_->SetSSRC(ssrc);
}

void RtpAudioStreamReceiver::OnRtpPacket(const RtpPacketReceived& packet) {
  int64_t now_ms = rtc::TimeMillis();
  uint8_t audio_level;
  bool voice_activity;
  bool has_audio_level =
      packet.GetExtension<::webrtc::AudioLevel>(&voice_activity, &audio_level);

  {
    rtc::CritScope cs(&rtp_sources_lock_);
    last_received_rtp_timestamp_ = packet.Timestamp();
    last_received_rtp_system_time_ms_ = now_ms;
    if (has_audio_level)
      last_received_rtp_audio_level_ = audio_level;
    std::vector<uint32_t> csrcs = packet.Csrcs();
    contributing_sources_.Update(now_ms, csrcs);
  }

#if 0
  // Store playout timestamp for the received RTP packet
  UpdatePlayoutTimestamp(false);
#endif
  const auto& it = payload_type_frequencies_.find(packet.PayloadType());
  if (it == payload_type_frequencies_.end())
    return;
  // TODO(nisse): Set payload_type_frequency earlier, when packet is parsed.
  RtpPacketReceived packet_copy(packet);
  packet_copy.set_payload_type_frequency(it->second);

  receive_statistics_->OnRtpPacket(packet_copy);

  rtc::ArrayView<const uint8_t> payload = packet_copy.payload();
  WebRtcRTPHeader webrtc_rtp_header = {};
  packet_copy.GetHeader(&webrtc_rtp_header.header);

  if (payload.size() == 0) {
    webrtc_rtp_header.frameType = kEmptyFrame;
  }

  if (!is_playing_) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.
    return;
  }

  // Push the incoming payload (parsed and ready for decoding) into the ACM
  if (audio_coding_->IncomingPacket(payload.data(), payload.size(),
                                    webrtc_rtp_header) != 0) {
    RTC_DLOG(LS_ERROR)
        << "Channel::OnReceivedPayloadData() unable to push data to the ACM";
    return;
  }

  int64_t round_trip_time = 0;

  // Passed on to RtcpReceiver.
  // TODO(nisse): Add a remote_ssrc_ member?
  rtp_rtcp_->RTT(remote_ssrc_, &round_trip_time, NULL, NULL, NULL);

  std::vector<uint16_t> nack_list = audio_coding_->GetNackList(round_trip_time);
  if (!nack_list.empty()) {
    // Passed on to RtcpSender
    rtp_rtcp_->SendNACK(nack_list.data(), static_cast<int>(nack_list.size()));
  }
}

void RtpAudioStreamReceiver::OnRtcpPacket(
    rtc::ArrayView<const uint8_t> packet) {
#if 0
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);
#endif

  rtp_rtcp_->IncomingRtcpPacket(packet.data(), packet.size());

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return;
  }

  // Invoke audio encoders OnReceivedRtt().
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder)
      (*encoder)->OnReceivedRtt(rtt);
  });

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (0 !=
      rtp_rtcp_->RemoteNTP(&ntp_secs, &ntp_frac, NULL, NULL, &rtp_timestamp)) {
    // Waiting for RTCP.
    return;
  }

  {
#if 0
    rtc::CritScope lock(&ts_stats_lock_);
#endif
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
}

std::vector<RtpSource> RtpAudioStreamReceiver::GetSources() const {
  int64_t now_ms = rtc::TimeMillis();
  std::vector<RtpSource> sources;
  {
    rtc::CritScope cs(&rtp_sources_lock_);
    sources = contributing_sources_.GetSources(now_ms);
    if (last_received_rtp_system_time_ms_ >=
        now_ms - ContributingSources::kHistoryMs) {
      sources.emplace_back(*last_received_rtp_system_time_ms_, remote_ssrc_,
                           RtpSourceType::SSRC);
      sources.back().set_audio_level(last_received_rtp_audio_level_);
    }
  }
  return sources;
}

RtpAudioStreamReceiver::Stats RtpAudioStreamReceiver::GetRtpStatistics() const {
  Stats stats = {0};
  // The jitter statistics is updated for each received RTP packet and is
  // based on received packets.
  RtcpStatistics statistics;
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(remote_ssrc_);
  if (statistician) {
    statistician->GetStatistics(&statistics,
                                rtp_rtcp_->RTCP() == RtcpMode::kOff);
  }

  stats.fractionLost = statistics.fraction_lost;
  stats.cumulativeLost = statistics.packets_lost;
  stats.extendedMax = statistics.extended_highest_sequence_number;
  stats.jitterSamples = statistics.jitter;

  // --- RTT
  stats.rttMs = GetRTT();

  // --- Data counters
  size_t bytesReceived(0);
  uint32_t packetsReceived(0);

  if (statistician) {
    statistician->GetDataCounters(&bytesReceived, &packetsReceived);
  }

  stats.bytesSent = 0;
  stats.packetsSent = 0;
  stats.bytesReceived = bytesReceived;
  stats.packetsReceived = packetsReceived;

#if 0
  // --- Timestamps
  {
    rtc::CritScope lock(&ts_stats_lock_);
    stats.capture_start_ntp_time_ms_ = capture_start_ntp_time_ms_;
  }
#endif

  return stats;
}

int64_t RtpAudioStreamReceiver::EstimateNtpMs(uint32_t rtp_timestamp) {
  return ntp_estimator_.Estimate(rtp_timestamp);
}

absl::optional<Syncable::Info> RtpAudioStreamReceiver::GetSyncInfo() const {
  Syncable::Info info;
  if (rtp_rtcp_->RemoteNTP(&info.capture_time_ntp_secs,
                           &info.capture_time_ntp_frac, nullptr, nullptr,
                           &info.capture_time_source_clock) != 0) {
    return absl::nullopt;
  }
  {
    rtc::CritScope cs(&rtp_sources_lock_);
    if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_ms_) {
      return absl::nullopt;
    }
    info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
    info.latest_receive_time_ms = *last_received_rtp_system_time_ms_;
  }
  return info;
}

absl::optional<uint32_t> RtpAudioStreamReceiver::GetRtpTimestamp() const {
  rtc::CritScope cs(&rtp_sources_lock_);
  return last_received_rtp_timestamp_;
}

void RtpAudioStreamReceiver::AssociateSendChannel(
    const voe::ChannelProxy* send_channel_proxy) {
  associated_send_channel_ = send_channel_proxy;
}

void RtpAudioStreamReceiver::DisassociateSendChannel() {
  associated_send_channel_ = nullptr;
}

int64_t RtpAudioStreamReceiver::GetRTT() const {
  RtcpMode method = rtp_rtcp_->RTCP();
  if (method == RtcpMode::kOff) {
    return 0;
  }
  std::vector<RTCPReportBlock> report_blocks;
  rtp_rtcp_->RemoteRTCPStat(&report_blocks);

  int64_t rtt = 0;
  if (report_blocks.empty()) {
#if 0
    rtc::CritScope lock(&assoc_send_channel_lock_);
#endif
    // Tries to get RTT from an associated channel. This is important for
    // receive-only channels.
    if (associated_send_channel_) {
      rtt = associated_send_channel_->GetRTT();
    }
    return rtt;
  }

  std::vector<RTCPReportBlock>::const_iterator it = report_blocks.begin();
  for (; it != report_blocks.end(); ++it) {
    if (it->sender_ssrc == remote_ssrc_)
      break;
  }

  // If we have not received packets with SSRC matching the report blocks, use
  // the SSRC of the first report block for calculating the RTT. This is very
  // important for send-only channels where we don't know the SSRC of the other
  // end.
  uint32_t ssrc =
      (it == report_blocks.end()) ? report_blocks[0].sender_ssrc : remote_ssrc_;

  int64_t avg_rtt = 0;
  int64_t max_rtt = 0;
  int64_t min_rtt = 0;
  if (rtp_rtcp_->RTT(ssrc, &rtt, &avg_rtt, &min_rtt, &max_rtt) != 0) {
    return 0;
  }
  return rtt;
}

}  // namespace webrtc
