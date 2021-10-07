/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_v3.h"

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"
#include "logging/rtc_event_log/encoder/var_int.h"
#include "logging/rtc_event_log/events/rtc_event_alr_state.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_begin_log.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "logging/rtc_event_log/events/rtc_event_end_log.h"
#include "logging/rtc_event_log/events/rtc_event_frame_decoded.h"
#include "logging/rtc_event_log/events/rtc_event_generic_ack_received.h"
#include "logging/rtc_event_log/events/rtc_event_generic_packet_received.h"
#include "logging/rtc_event_log/events/rtc_event_generic_packet_sent.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_remote_estimate.h"
#include "logging/rtc_event_log/events/rtc_event_route_change.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

using webrtc_event_logging::ToUnsigned;

namespace webrtc {

std::string RtcEventLogEncoderV3::EncodeLogStart(int64_t timestamp_us,
                                                 int64_t utc_time_us) {
  std::unique_ptr<RtcEventBeginLog> begin_log =
      std::make_unique<RtcEventBeginLog>(Timestamp::Micros(timestamp_us),
                                         Timestamp::Micros(utc_time_us));
  std::vector<const RtcEvent*> batch;
  batch.push_back(begin_log.get());

  std::string encoded_event = RtcEventBeginLog::Encode(batch);

  return encoded_event;
}

std::string RtcEventLogEncoderV3::EncodeLogEnd(int64_t timestamp_us) {
  std::unique_ptr<RtcEventEndLog> end_log =
      std::make_unique<RtcEventEndLog>(Timestamp::Micros(timestamp_us));
  std::vector<const RtcEvent*> batch;
  batch.push_back(end_log.get());

  std::string encoded_event = RtcEventEndLog::Encode(batch);

  return encoded_event;
}

RtcEventLogEncoderV3::RtcEventLogEncoderV3() {
  encoders_[static_cast<uint32_t>(RtcEvent::Type::AlrStateEvent)] =
      RtcEventAlrState::Encode;
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::AudioNetworkAdaptation] =
  //  RtcEventAudioNetworkAdaptation::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::AudioPlayout] =
  //  RtcEventAudioPlayout::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::AudioReceiveStreamConfig]
  //  = RtcEventAudioReceiveStreamConfig::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::AudioSendStreamConfig] =
  //  RtcEventAudioSendStreamConfig::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::BweUpdateDelayBased] =
  //  RtcEventBweUpdateDelayBased::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::BweUpdateLossBased] =
  //  RtcEventBweUpdateLossBased::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::DtlsTransportState] =
  //  RtcEventDtlsTransportState::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::DtlsWritableState] =
  //  RtcEventDtlsWritableState::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::FrameDecoded] =
  //  RtcEventFrameDecoded::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::GenericAckReceived] =
  //  RtcEventGenericAckReceived::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::GenericPacketReceived] =
  //  RtcEventGenericPacketReceived::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::GenericPacketSent] =
  //  RtcEventGenericPacketSent::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::IceCandidatePairConfig] =
  //  RtcEventIceCandidatePairConfig::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::IceCandidatePairEvent] =
  //  RtcEventIceCandidatePair::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::ProbeClusterCreated] =
  //  RtcEventProbeClusterCreated::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::ProbeResultFailure] =
  //  RtcEventProbeResultFailure::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::ProbeResultSuccess] =
  //  RtcEventProbeResultSuccess::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::RemoteEstimateEvent] =
  //  RtcEventRemoteEstimate::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::RouteChangeEvent] =
  //  RtcEventRouteChange::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::RtcpPacketIncoming] =
  //  RtcEventRtcpPacketIncoming::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::RtcpPacketOutgoing] =
  //  RtcEventRtcpPacketOutgoing::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::RtpPacketIncoming] =
  //  RtcEventRtpPacketIncoming::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::RtpPacketOutgoing] =
  //  RtcEventRtpPacketOutgoing::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::VideoReceiveStreamConfig]
  //  = RtcEventVideoReceiveStreamConfig::CreateEncoder();
  //  encoders_[static_cast<uint32_t>(RtcEvent::Type::VideoSendStreamConfig] =
  //  RtcEventVideoSendStreamConfig::CreateEncoder();
}

std::string RtcEventLogEncoderV3::EncodeBatch(
    std::deque<std::unique_ptr<RtcEvent>>::const_iterator begin,
    std::deque<std::unique_ptr<RtcEvent>>::const_iterator end) {
  std::string encoded_output;

  std::map<uint64_t, std::vector<const RtcEvent*>> event_groups;

  for (auto it = begin; it != end; ++it) {
    uint64_t key = (*it)->GetGroupKey();
    key = (key << 32) + static_cast<uint64_t>((*it)->GetType());
    event_groups[key].push_back(it->get());
  }

  for (auto& kv : event_groups) {
    uint32_t event_type = kv.first & 0xFFFFFFFF;
    auto encoder_it = encoders_.find(event_type);
    RTC_DCHECK(encoder_it != encoders_.end());
    if (encoder_it != encoders_.end()) {
      auto& encoder = encoder_it->second;
      std::string encoding = encoder(kv.second);
      // TODO(terelius): Use some "string builder" or preallocate?
      encoded_output += encoding;
    }
  }

  return encoded_output;
}

}  // namespace webrtc
