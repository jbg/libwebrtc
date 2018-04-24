/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <iostream>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/congestion_controller/transport_feedback_adapter.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// Return default values for header extensions, to use on streams without stored
// mapping data. Currently this only applies to audio streams, since the mapping
// is not stored in the event log.
// TODO(ivoc): Remove this once this mapping is stored in the event log for
//             audio streams. Tracking bug: webrtc:6399
webrtc::RtpHeaderExtensionMap GetDefaultHeaderExtensionMap() {
  webrtc::RtpHeaderExtensionMap default_map;
  default_map.Register<AudioLevel>(webrtc::RtpExtension::kAudioLevelDefaultId);
  default_map.Register<TransmissionOffset>(
      webrtc::RtpExtension::kTimestampOffsetDefaultId);
  default_map.Register<AbsoluteSendTime>(
      webrtc::RtpExtension::kAbsSendTimeDefaultId);
  default_map.Register<VideoOrientation>(
      webrtc::RtpExtension::kVideoRotationDefaultId);
  default_map.Register<VideoContentTypeExtension>(
      webrtc::RtpExtension::kVideoContentTypeDefaultId);
  default_map.Register<VideoTimingExtension>(
      webrtc::RtpExtension::kVideoTimingDefaultId);
  default_map.Register<TransportSequenceNumber>(
      webrtc::RtpExtension::kTransportSequenceNumberDefaultId);
  default_map.Register<PlayoutDelayLimits>(
      webrtc::RtpExtension::kPlayoutDelayDefaultId);
  return default_map;
}

// Packets without sequence number
struct SentPacket {
  uint64_t timestamp_us;
  size_t size;
  uint8_t payload_type;
};
void PrintHeader() {
  printf("packet_size send_time recv_time feed_time\n");
}
void FormatOut(int64_t size,
               double send_time,
               double recv_time = nan(""),
               double feedback_time = nan("")) {
  printf("%ld %.6f %.6f %.6f\n", size, send_time, recv_time, feedback_time);
}

void ParseLog(const ParsedRtcEventLog& parsed_log_) {
  SimulatedClock clock(10000);
  TransportFeedbackAdapter feedback_adapter(&clock);

  webrtc::RtpHeaderExtensionMap default_extension_map =
      GetDefaultHeaderExtensionMap();

  uint8_t last_incoming_rtcp_packet[IP_PACKET_SIZE];
  uint8_t last_incoming_rtcp_packet_length = 0;
  int16_t last_seq_num = 0;

  std::vector<SentPacket> curr_untracked;
  std::map<int32_t, std::vector<SentPacket>> all_untracked;
  std::map<int32_t, SentPacket> all_tracked;
  PrintHeader();
  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    PacketDirection direction;
    uint8_t header_data[IP_PACKET_SIZE];
    size_t header_length;
    size_t total_length;

    if (parsed_log_.GetEventType(i) == ParsedRtcEventLog::RTP_EVENT) {
      RtpHeaderExtensionMap* extension_map = parsed_log_.GetRtpHeader(
          i, &direction, header_data, &header_length, &total_length, nullptr);
      if (direction != PacketDirection::kOutgoingPacket)
        continue;
      RtpUtility::RtpHeaderParser rtp_parser(header_data, header_length);
      RTPHeader parsed_header;
      if (extension_map != nullptr) {
        rtp_parser.Parse(&parsed_header, extension_map);
      } else {
        // Use the default extension map.
        rtp_parser.Parse(&parsed_header, &default_extension_map);
      }

      uint64_t timestamp_us = parsed_log_.GetTimestamp(i);
      if (parsed_header.extension.hasTransportSequenceNumber) {
        uint16_t seq_num = parsed_header.extension.transportSequenceNumber;
        if (++last_seq_num != seq_num)
          break;
        feedback_adapter.AddPacket(parsed_header.ssrc, seq_num, total_length,
                                   PacedPacketInfo());
        feedback_adapter.OnSentPacket(seq_num, timestamp_us / 1000);

        all_tracked[seq_num] = {timestamp_us, total_length,
                                parsed_header.payloadType};
        all_untracked[seq_num] = std::move(curr_untracked);
        curr_untracked = std::vector<SentPacket>();
      } else {
        curr_untracked.push_back(
            {timestamp_us, total_length, parsed_header.payloadType});
      }
    } else if (parsed_log_.GetEventType(i) == ParsedRtcEventLog::RTCP_EVENT) {
      uint8_t packet[IP_PACKET_SIZE];
      parsed_log_.GetRtcpPacket(i, &direction, packet, &total_length);
      // Currently incoming RTCP packets are logged twice, both for audio and
      // video. Only act on one of them. Compare against the previous parsed
      // incoming RTCP packet.
      if (direction != webrtc::kIncomingPacket)
        continue;
      RTC_CHECK_LE(total_length, IP_PACKET_SIZE);
      if (total_length == last_incoming_rtcp_packet_length &&
          memcmp(last_incoming_rtcp_packet, packet, total_length) == 0) {
        continue;
      } else {
        memcpy(last_incoming_rtcp_packet, packet, total_length);
        last_incoming_rtcp_packet_length = total_length;
      }
      rtcp::CommonHeader header;
      const uint8_t* packet_end = packet + total_length;
      for (const uint8_t* block = packet; block < packet_end;
           block = header.NextPacket()) {
        RTC_CHECK(header.Parse(block, packet_end - block));
        if (header.type() == rtcp::TransportFeedback::kPacketType &&
            header.fmt() == rtcp::TransportFeedback::kFeedbackMessageType) {
          rtcp::TransportFeedback rtcp_packet;
          if (rtcp_packet.Parse(header)) {
            feedback_adapter.OnTransportFeedback(rtcp_packet);
            std::vector<PacketFeedback> feedbacks =
                feedback_adapter.GetTransportFeedbackVector();

            double feedback_time = parsed_log_.GetTimestamp(i) * 1e-6;

            for (size_t i = 0; i < feedbacks.size(); ++i) {
              auto& fb = feedbacks[i];
              for (auto& sent : all_untracked[fb.sequence_number]) {
                FormatOut(sent.size, sent.timestamp_us * 1e-6);
              }
              auto& sent = all_tracked[fb.sequence_number];
              if (sent.timestamp_us == 0) {
                RTC_LOG(LS_WARNING) << "Invalid send time";
                break;
              }
              auto send_time = sent.timestamp_us * 1e-6;
              // printf("payload: %i :: ", sent.payload_type);
              auto recv_ms = fb.arrival_time_ms;
              double recv_us = recv_ms == -1 ? INFINITY : recv_ms * 1e-3;
              if (i == feedbacks.size() - 1)
                FormatOut(sent.size, send_time, recv_us, feedback_time);
              else
                FormatOut(sent.size, send_time, recv_us);
              all_untracked.erase(fb.sequence_number);
            }
          }
        }
      }
    }
  }
}
}  // namespace
}  // namespace webrtc

void PrintHelp() {
  printf("Extracts sent packets and their feedback from rtc event logs\n");
  printf("Usage: input log using filename or to stdin\n");
  printf("Output, space separated values for each sent packet.\n");
  printf("Format: ");
  printf("packet size [bytes], send time [s], ");
  printf("recv time [s/nan/inf], feedback time [s/nan]\n");
}

int main(int argc, char* argv[]) {
  webrtc::ParsedRtcEventLog parsed_log;
  if (argc == 2) {
    if (std::string("--help") == argv[1]) {
      PrintHelp();
    } else {
      parsed_log.ParseFile(argv[1]);
    }
  } else {
    parsed_log.ParseStream(std::cin);
  }
  webrtc::ParseLog(parsed_log);
  return 0;
}
