/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_reencoder/reencode.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/rtc_event_log_output_file.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/transport/network_types.h"
#include "logging/rtc_event_log/events/rtc_event_alr_state.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "logging/rtc_event_log/events/rtc_event_frame_decoded.h"
#include "logging/rtc_event_log/events/rtc_event_generic_ack_received.h"
#include "logging/rtc_event_log/events/rtc_event_generic_packet_received.h"
#include "logging/rtc_event_log/events/rtc_event_generic_packet_sent.h"
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
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_event_processor.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/fake_clock.h"

namespace webrtc {
namespace {

webrtc::RtpHeaderExtensionMap GetHeaderExtensions(
    const webrtc::rtclog::StreamConfig& config) {
  webrtc::RtpHeaderExtensionMap extensions;
  for (const auto& extension : config.rtp_extensions) {
    extensions.RegisterByUri(extension.id, extension.uri);
  }
  return extensions;
}

}  // namespace

bool Reencode(
    std::string inputfile,
    std::string outputfile,
    ParsedRtcEventLog::UnconfiguredHeaderExtensions unconfigured_extensions,
    RtcEventLog::EncodingType encoding_type) {
  ParsedRtcEventLog parsed_log(unconfigured_extensions,
                               /*allow_incomplete_logs*/ true);

  auto status = parsed_log.ParseFile(inputfile);
  if (!status.ok()) {
    std::cerr << "Failed to parse " << inputfile << ": " << status.message()
              << std::endl;
    return false;
  }

  // Clock must be declared before the event log to ensure that the clock is
  // still alive if logging is stopped by running the event log destructor.
  rtc::ScopedFakeClock clock;
  std::map<uint32_t, RtpHeaderExtensionMap> header_extensions_by_ssrc;

  std::unique_ptr<TaskQueueFactory> task_queue_factory =
      CreateDefaultTaskQueueFactory();
  RtcEventLogFactory rtc_event_log_factory(task_queue_factory.get());
  std::unique_ptr<RtcEventLog> reencoded_log =
      rtc_event_log_factory.CreateRtcEventLog(encoding_type);

  auto audio_recv_stream_handler = [&clock, &reencoded_log,
                                    &header_extensions_by_ssrc](
                                       const LoggedAudioRecvConfig& event) {
    clock.SetTime(Timestamp::Micros(event.log_time_us()));
    auto config = std::make_unique<rtclog::StreamConfig>(event.config);
    header_extensions_by_ssrc[config->remote_ssrc] =
        GetHeaderExtensions(*config);
    reencoded_log->Log(
        std::make_unique<RtcEventAudioReceiveStreamConfig>(std::move(config)));
  };

  auto audio_send_stream_handler =
      [&clock, &reencoded_log,
       &header_extensions_by_ssrc](const LoggedAudioSendConfig& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        auto config = std::make_unique<rtclog::StreamConfig>(event.config);
        header_extensions_by_ssrc[config->local_ssrc] =
            GetHeaderExtensions(*config);
        reencoded_log->Log(
            std::make_unique<RtcEventAudioSendStreamConfig>(std::move(config)));
      };

  auto video_recv_stream_handler = [&clock, &reencoded_log,
                                    &header_extensions_by_ssrc](
                                       const LoggedVideoRecvConfig& event) {
    clock.SetTime(Timestamp::Micros(event.log_time_us()));
    auto config = std::make_unique<rtclog::StreamConfig>(event.config);
    header_extensions_by_ssrc[config->remote_ssrc] =
        GetHeaderExtensions(*config);
    reencoded_log->Log(
        std::make_unique<RtcEventVideoReceiveStreamConfig>(std::move(config)));
  };

  auto video_send_stream_handler =
      [&clock, &reencoded_log,
       &header_extensions_by_ssrc](const LoggedVideoSendConfig& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        auto config = std::make_unique<rtclog::StreamConfig>(event.config);
        header_extensions_by_ssrc[config->local_ssrc] =
            GetHeaderExtensions(*config);
        reencoded_log->Log(
            std::make_unique<RtcEventVideoSendStreamConfig>(std::move(config)));
      };

  auto start_logging_handler = [&clock, &reencoded_log,
                                outputfile](const LoggedStartEvent& event) {
    clock.SetTime(Timestamp::Micros(event.log_time_us()));
    reencoded_log->StartLogging(
        std::make_unique<RtcEventLogOutputFile>(outputfile, 100000000),
        RtcEventLog::kImmediateOutput);
  };

  auto stop_logging_handler = [&clock,
                               &reencoded_log](const LoggedStopEvent& event) {
    clock.SetTime(Timestamp::Micros(event.log_time_us()));
    reencoded_log->StopLogging();
  };

  auto alr_state_handler = [&clock,
                            &reencoded_log](const LoggedAlrStateEvent& event) {
    clock.SetTime(Timestamp::Micros(event.log_time_us()));
    reencoded_log->Log(std::make_unique<RtcEventAlrState>(event.in_alr));
  };

  auto audio_playout_handler =
      [&clock, &reencoded_log](const LoggedAudioPlayoutEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventAudioPlayout>(event.ssrc));
      };

  auto audio_network_adaptation_handler =
      [&clock, &reencoded_log](const LoggedAudioNetworkAdaptationEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        auto config = std::make_unique<AudioEncoderRuntimeConfig>(event.config);
        reencoded_log->Log(std::make_unique<RtcEventAudioNetworkAdaptation>(
            std::move(config)));
      };

  auto bwe_probe_cluster_created_handler =
      [&clock, &reencoded_log](const LoggedBweProbeClusterCreatedEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventProbeClusterCreated>(
            event.id, event.bitrate_bps, event.min_packets, event.min_bytes));
      };

  auto bwe_probe_failure_handler =
      [&clock, &reencoded_log](const LoggedBweProbeFailureEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventProbeResultFailure>(
            event.id, event.failure_reason));
      };

  auto bwe_probe_success_handler =
      [&clock, &reencoded_log](const LoggedBweProbeSuccessEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventProbeResultSuccess>(
            event.id, event.bitrate_bps));
      };

  auto bwe_delay_update_handler =
      [&clock, &reencoded_log](const LoggedBweDelayBasedUpdate& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventBweUpdateDelayBased>(
            event.bitrate_bps, event.detector_state));
      };

  auto bwe_loss_update_handler =
      [&clock, &reencoded_log](const LoggedBweLossBasedUpdate& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventBweUpdateLossBased>(
            event.bitrate_bps, event.fraction_lost, event.expected_packets));
      };

  auto dtls_transport_state_handler =
      [&clock, &reencoded_log](const LoggedDtlsTransportState& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventDtlsTransportState>(
            event.dtls_transport_state));
      };

  auto dtls_transport_writable_handler =
      [&clock, &reencoded_log](const LoggedDtlsWritableState& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(
            std::make_unique<RtcEventDtlsWritableState>(event.writable));
      };

  auto ice_candidate_pair_config_handler =
      [&clock, &reencoded_log](const LoggedIceCandidatePairConfig& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        IceCandidatePairDescription desc;
        desc.local_candidate_type = event.local_candidate_type;
        desc.local_relay_protocol = event.local_relay_protocol;
        desc.local_network_type = event.local_network_type;
        desc.local_address_family = event.local_address_family;
        desc.remote_candidate_type = event.remote_candidate_type;
        desc.remote_address_family = event.remote_address_family;
        desc.candidate_pair_protocol = event.candidate_pair_protocol;

        reencoded_log->Log(std::make_unique<RtcEventIceCandidatePairConfig>(
            event.type, event.candidate_pair_id, desc));
      };

  auto ice_candidate_pair_event_handler =
      [&clock, &reencoded_log](const LoggedIceCandidatePairEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventIceCandidatePair>(
            event.type, event.candidate_pair_id, event.transaction_id));
      };

  auto route_change_handler =
      [&clock, &reencoded_log](const LoggedRouteChangeEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventRouteChange>(
            event.connected, event.overhead));
      };

  auto remote_estimate_handler =
      [&clock, &reencoded_log](const LoggedRemoteEstimateEvent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventRemoteEstimate>(
            event.link_capacity_lower.value_or(DataRate::Infinity()),
            event.link_capacity_upper.value_or(DataRate::Infinity())));
      };

  auto incoming_rtp_packet_handler =
      [&clock, &reencoded_log,
       &header_extensions_by_ssrc](const LoggedRtpPacketIncoming& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        auto it = header_extensions_by_ssrc.find(event.rtp.header.ssrc);
        if (it == header_extensions_by_ssrc.end()) {
          RtpHeaderExtensionMap default_map =
              ParsedRtcEventLog::GetDefaultHeaderExtensionMap();
          std::tie(it, std::ignore) = header_extensions_by_ssrc.insert(

              {event.rtp.header.ssrc, default_map});
        }
        RtpPacketReceived packet(&(it->second));
        packet.SetMarker(event.rtp.header.markerBit);
        packet.SetPayloadType(event.rtp.header.payloadType);
        packet.SetSequenceNumber(event.rtp.header.sequenceNumber);
        packet.SetTimestamp(event.rtp.header.timestamp);
        packet.SetSsrc(event.rtp.header.ssrc);
        if (event.rtp.header.extension.hasAbsoluteSendTime) {
          bool success = packet.SetExtension<AbsoluteSendTime>(
              event.rtp.header.extension.absoluteSendTime);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasTransmissionTimeOffset) {
          bool success = packet.SetExtension<TransmissionOffset>(
              event.rtp.header.extension.transmissionTimeOffset);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasAudioLevel) {
          bool success = packet.SetExtension<AudioLevel>(
              event.rtp.header.extension.voiceActivity,
              event.rtp.header.extension.audioLevel);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasVideoRotation) {
          bool success = packet.SetExtension<VideoOrientation>(
              event.rtp.header.extension.videoRotation);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasTransportSequenceNumber) {
          bool success = packet.SetExtension<TransportSequenceNumber>(
              event.rtp.header.extension.transportSequenceNumber);
          RTC_DCHECK(success);
        }
        // Fake header of the correct size by adding in CSRCs
        RTC_DCHECK(packet.headers_size() % 4 == 0);
        RTC_DCHECK(event.rtp.header_length % 4 == 0);
        RTC_DCHECK(packet.headers_size() % 4 == event.rtp.header_length % 4);
        // int num_csrcs = (event.rtp.header_length - packet.headers_size()) /
        // 4; packet.SetCsrcs(std::vector<uint32_t>(num_csrcs));

        packet.SetPayloadSize(event.rtp.total_length - event.rtp.header_length -
                              event.rtp.header.paddingLength);
        packet.SetPadding(event.rtp.header.paddingLength);
        reencoded_log->Log(std::make_unique<RtcEventRtpPacketIncoming>(packet));
      };

  auto outgoing_rtp_packet_handler =
      [&clock, &reencoded_log,
       &header_extensions_by_ssrc](const LoggedRtpPacketOutgoing& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        auto it = header_extensions_by_ssrc.find(event.rtp.header.ssrc);
        if (it == header_extensions_by_ssrc.end()) {
          RtpHeaderExtensionMap default_map =
              ParsedRtcEventLog::GetDefaultHeaderExtensionMap();
          std::tie(it, std::ignore) = header_extensions_by_ssrc.insert(

              {event.rtp.header.ssrc, default_map});
        }
        RtpPacketToSend packet(&(it->second));
        packet.SetMarker(event.rtp.header.markerBit);
        packet.SetPayloadType(event.rtp.header.payloadType);
        packet.SetSequenceNumber(event.rtp.header.sequenceNumber);
        packet.SetTimestamp(event.rtp.header.timestamp);
        packet.SetSsrc(event.rtp.header.ssrc);
        if (event.rtp.header.extension.hasAbsoluteSendTime) {
          bool success = packet.SetExtension<AbsoluteSendTime>(
              event.rtp.header.extension.absoluteSendTime);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasTransmissionTimeOffset) {
          bool success = packet.SetExtension<TransmissionOffset>(
              event.rtp.header.extension.transmissionTimeOffset);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasAudioLevel) {
          bool success = packet.SetExtension<AudioLevel>(
              event.rtp.header.extension.voiceActivity,
              event.rtp.header.extension.audioLevel);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasVideoRotation) {
          bool success = packet.SetExtension<VideoOrientation>(
              event.rtp.header.extension.videoRotation);
          RTC_DCHECK(success);
        }
        if (event.rtp.header.extension.hasTransportSequenceNumber) {
          bool success = packet.SetExtension<TransportSequenceNumber>(
              event.rtp.header.extension.transportSequenceNumber);
          RTC_DCHECK(success);
        }
        // Fake header of the correct size by adding in CSRCs
        RTC_DCHECK(packet.headers_size() % 4 == 0);
        RTC_DCHECK(event.rtp.header_length % 4 == 0);
        RTC_DCHECK(packet.headers_size() % 4 == event.rtp.header_length % 4);
        // int num_csrcs = (event.rtp.header_length - packet.headers_size()) /
        // 4; packet.SetCsrcs(std::vector<uint32_t>(num_csrcs));

        packet.SetPayloadSize(event.rtp.total_length - event.rtp.header_length -
                              event.rtp.header.paddingLength);
        packet.SetPadding(event.rtp.header.paddingLength);
        reencoded_log->Log(std::make_unique<RtcEventRtpPacketOutgoing>(
            packet, PacedPacketInfo::kNotAProbe /* Not used */));
      };

  auto incoming_rtcp_packet_handler =
      [&clock, &reencoded_log](const LoggedRtcpPacketIncoming& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(
            std::make_unique<RtcEventRtcpPacketIncoming>(event.rtcp.raw_data));
      };

  auto outgoing_rtcp_packet_handler =
      [&clock, &reencoded_log](const LoggedRtcpPacketOutgoing& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(
            std::make_unique<RtcEventRtcpPacketOutgoing>(event.rtcp.raw_data));
      };

  auto generic_packet_received_handler =
      [&clock, &reencoded_log](const LoggedGenericPacketReceived& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventGenericPacketReceived>(
            event.packet_number, event.packet_length));
      };

  auto generic_packet_sent_handler =
      [&clock, &reencoded_log](const LoggedGenericPacketSent& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventGenericPacketSent>(
            event.packet_number, event.overhead_length, event.payload_length,
            event.padding_length));
      };

  auto generic_ack_received_handler =
      [&clock, &reencoded_log](const LoggedGenericAckReceived& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        std::vector<AckedPacket> acked_packets;
        acked_packets.push_back(
            {event.acked_packet_number, event.receive_acked_packet_time_ms});
        auto acked_packet_events = RtcEventGenericAckReceived::CreateLogs(
            event.packet_number, acked_packets);
        RTC_DCHECK(acked_packet_events.size() == 1);
        reencoded_log->Log(std::move(acked_packet_events[0]));
      };

  auto decoded_frame_handler =
      [&clock, &reencoded_log](const LoggedFrameDecoded& event) {
        clock.SetTime(Timestamp::Micros(event.log_time_us()));
        reencoded_log->Log(std::make_unique<RtcEventFrameDecoded>(
            event.render_time_ms, event.ssrc, event.width, event.height,
            event.codec, event.qp));
      };

  RtcEventProcessor processor;

  processor.AddEvents(parsed_log.audio_recv_configs(),
                      audio_recv_stream_handler);
  processor.AddEvents(parsed_log.audio_send_configs(),
                      audio_send_stream_handler);
  processor.AddEvents(parsed_log.video_recv_configs(),
                      video_recv_stream_handler);
  processor.AddEvents(parsed_log.video_send_configs(),
                      video_send_stream_handler);

  processor.AddEvents(parsed_log.start_log_events(), start_logging_handler);
  processor.AddEvents(parsed_log.stop_log_events(), stop_logging_handler);

  processor.AddEvents(parsed_log.alr_state_events(), alr_state_handler);

  for (const auto& kv : parsed_log.audio_playout_events()) {
    processor.AddEvents(kv.second, audio_playout_handler);
  }

  processor.AddEvents(parsed_log.audio_network_adaptation_events(),
                      audio_network_adaptation_handler);
  processor.AddEvents(parsed_log.bwe_probe_cluster_created_events(),
                      bwe_probe_cluster_created_handler);
  processor.AddEvents(parsed_log.bwe_probe_failure_events(),
                      bwe_probe_failure_handler);
  processor.AddEvents(parsed_log.bwe_probe_success_events(),
                      bwe_probe_success_handler);

  processor.AddEvents(parsed_log.bwe_delay_updates(), bwe_delay_update_handler);
  processor.AddEvents(parsed_log.bwe_loss_updates(), bwe_loss_update_handler);

  processor.AddEvents(parsed_log.dtls_transport_states(),
                      dtls_transport_state_handler);
  processor.AddEvents(parsed_log.dtls_writable_states(),
                      dtls_transport_writable_handler);
  processor.AddEvents(parsed_log.ice_candidate_pair_configs(),
                      ice_candidate_pair_config_handler);
  processor.AddEvents(parsed_log.ice_candidate_pair_events(),
                      ice_candidate_pair_event_handler);
  processor.AddEvents(parsed_log.route_change_events(), route_change_handler);
  processor.AddEvents(parsed_log.remote_estimate_events(),
                      remote_estimate_handler);

  for (const auto& stream : parsed_log.incoming_rtp_packets_by_ssrc()) {
    processor.AddEvents(stream.incoming_packets, incoming_rtp_packet_handler);
  }
  for (const auto& stream : parsed_log.outgoing_rtp_packets_by_ssrc()) {
    processor.AddEvents(stream.outgoing_packets, outgoing_rtp_packet_handler);
  }

  processor.AddEvents(parsed_log.incoming_rtcp_packets(),
                      incoming_rtcp_packet_handler);

  processor.AddEvents(parsed_log.outgoing_rtcp_packets(),
                      outgoing_rtcp_packet_handler);

  processor.AddEvents(parsed_log.generic_packets_received(),
                      generic_packet_received_handler);
  processor.AddEvents(parsed_log.generic_packets_sent(),
                      generic_packet_sent_handler);
  processor.AddEvents(parsed_log.generic_acks_received(),
                      generic_ack_received_handler);

  for (const auto& kv : parsed_log.decoded_frames()) {
    processor.AddEvents(kv.second, decoded_frame_handler);
  }

  processor.ProcessEventsInOrder();

  return true;
}

}  // namespace webrtc
