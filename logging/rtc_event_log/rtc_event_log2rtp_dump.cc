/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "logging/rtc_event_log/rtc_event_processor.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "test/rtp_file_writer.h"

namespace {

using MediaType = webrtc::ParsedRtcEventLogNew::MediaType;

DEFINE_bool(
    audio,
    true,
    "Use --noaudio to exclude audio packets from the converted RTPdump file.");
DEFINE_bool(
    video,
    true,
    "Use --novideo to exclude video packets from the converted RTPdump file.");
DEFINE_bool(
    data,
    true,
    "Use --nodata to exclude data packets from the converted RTPdump file.");
DEFINE_bool(
    rtp,
    true,
    "Use --nortp to exclude RTP packets from the converted RTPdump file.");
DEFINE_bool(
    rtcp,
    true,
    "Use --nortcp to exclude RTCP packets from the converted RTPdump file.");
DEFINE_string(ssrc,
              "",
              "Store only packets with this SSRC (decimal or hex, the latter "
              "starting with 0x).");
DEFINE_bool(help, false, "Prints this message.");

// Parses the input string for a valid SSRC. If a valid SSRC is found, it is
// written to the output variable |ssrc|, and true is returned. Otherwise,
// false is returned.
// The empty string must be validated as true, because it is the default value
// of the command-line flag. In this case, no value is written to the output
// variable.
bool ParseSsrc(std::string str, uint32_t* ssrc) {
  // If the input string starts with 0x or 0X it indicates a hexadecimal number.
  auto read_mode = std::dec;
  if (str.size() > 2 &&
      (str.substr(0, 2) == "0x" || str.substr(0, 2) == "0X")) {
    read_mode = std::hex;
    str = str.substr(2);
  }
  std::stringstream ss(str);
  ss >> read_mode >> *ssrc;
  return str.empty() || (!ss.fail() && ss.eof());
}

}  // namespace

// This utility will convert a stored event log to the rtpdump format.
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool for converting an RtcEventLog file to an RTP dump file.\n"
      "Run " +
      program_name +
      " --help for usage.\n"
      "Example usage:\n" +
      program_name + " input.rel output.rtp\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) || FLAG_help ||
      argc != 3) {
    std::cout << usage;
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }

  std::string input_file = argv[1];
  std::string output_file = argv[2];

  uint32_t ssrc_filter = 0;
  if (strlen(FLAG_ssrc) > 0)
    RTC_CHECK(ParseSsrc(FLAG_ssrc, &ssrc_filter))
        << "Flag verification has failed.";

  webrtc::ParsedRtcEventLogNew parsed_stream;
  if (!parsed_stream.ParseFile(input_file)) {
    std::cerr << "Error while parsing input file: " << input_file << std::endl;
    return -1;
  }

  std::unique_ptr<webrtc::test::RtpFileWriter> rtp_writer(
      webrtc::test::RtpFileWriter::Create(
          webrtc::test::RtpFileWriter::FileFormat::kRtpDump, output_file));

  if (!rtp_writer.get()) {
    std::cerr << "Error while opening output file: " << output_file
              << std::endl;
    return -1;
  }

  std::cout << "Found " << parsed_stream.GetNumberOfEvents()
            << " events in the input file." << std::endl;
  int rtp_counter = 0, rtcp_counter = 0;
  bool header_only = false;

  webrtc::RtpHeaderExtensionMap default_extension_map =
      webrtc::ParsedRtcEventLogNew::GetDefaultHeaderExtensionMap();
  auto handle_rtp = [&default_extension_map, &rtp_writer, &rtp_counter](
                        const webrtc::LoggedRtpPacketIncoming& incoming) {
    webrtc::RtpPacket intermediary(&default_extension_map);

    intermediary.SetMarker(incoming.rtp.header.markerBit);
    intermediary.SetPayloadType(incoming.rtp.header.payloadType);
    intermediary.SetSequenceNumber(incoming.rtp.header.sequenceNumber);
    intermediary.SetTimestamp(incoming.rtp.header.timestamp);
    intermediary.SetSsrc(incoming.rtp.header.ssrc);
    if (incoming.rtp.header.numCSRCs > 0) {
      intermediary.SetCsrcs(rtc::ArrayView<const uint32_t>(
          incoming.rtp.header.arrOfCSRCs, incoming.rtp.header.numCSRCs));
    }

    // Set extensions.
    if (incoming.rtp.header.extension.hasTransmissionTimeOffset)
      intermediary.SetExtension<webrtc::TransmissionOffset>(
          incoming.rtp.header.extension.transmissionTimeOffset);
    if (incoming.rtp.header.extension.hasAbsoluteSendTime)
      intermediary.SetExtension<webrtc::AbsoluteSendTime>(
          incoming.rtp.header.extension.absoluteSendTime);
    if (incoming.rtp.header.extension.hasTransportSequenceNumber)
      intermediary.SetExtension<webrtc::TransportSequenceNumber>(
          incoming.rtp.header.extension.transportSequenceNumber);
    if (incoming.rtp.header.extension.hasAudioLevel)
      intermediary.SetExtension<webrtc::AudioLevel>(
          incoming.rtp.header.extension.voiceActivity,
          incoming.rtp.header.extension.audioLevel);
    if (incoming.rtp.header.extension.hasVideoRotation)
      intermediary.SetExtension<webrtc::VideoOrientation>(
          incoming.rtp.header.extension.videoRotation);
    if (incoming.rtp.header.extension.hasVideoContentType)
      intermediary.SetExtension<webrtc::VideoContentTypeExtension>(
          incoming.rtp.header.extension.videoContentType);
    if (incoming.rtp.header.extension.has_video_timing)
      intermediary.SetExtension<webrtc::VideoTimingExtension>(
          incoming.rtp.header.extension.video_timing);

    webrtc::test::RtpPacket packet;
    RTC_DCHECK_EQ(intermediary.size(), incoming.rtp.header_length);
    RTC_DCHECK_EQ(intermediary.headers_size(), incoming.rtp.header_length);
    memcpy(packet.data, intermediary.data(), intermediary.headers_size());
    packet.length = intermediary.headers_size();
    packet.original_length = incoming.rtp.total_length;
    packet.time_ms = incoming.log_time_ms();
    // Set padding bit.
    if (incoming.rtp.header.paddingLength > 0)
      packet.data[0] = packet.data[0] | 0x20;

    rtp_writer->WritePacket(&packet);
    rtp_counter++;
  };

  auto handle_rtcp = [&rtp_writer, &rtcp_counter](
                         const webrtc::LoggedRtcpPacketIncoming& incoming) {
    // Note that |packet_ssrc| is the sender SSRC. An RTCP message may contain
    // report blocks for many streams, thus several SSRCs and they doen't
    // necessarily have to be of the same media type. We therefore don't
    // support filtering of RTCP based on SSRC and media type.

    webrtc::test::RtpPacket packet;
    memcpy(packet.data, incoming.rtcp.raw_data.data(),
           incoming.rtcp.raw_data.size());
    packet.length = incoming.rtcp.raw_data.size();
    // For RTCP packets the original_length should be set to 0 in the
    // RTPdump format.
    packet.original_length = 0;
    packet.time_ms = incoming.log_time_ms();

    rtp_writer->WritePacket(&packet);
    rtcp_counter++;
  };

  webrtc::RtcEventProcessor event_processor;
  for (const auto& stream : parsed_stream.incoming_rtp_packets_by_ssrc()) {
    MediaType media_type =
        parsed_stream.GetMediaType(stream.ssrc, webrtc::kIncomingPacket);
    if (!FLAG_audio && media_type == MediaType::AUDIO)
      continue;
    if (!FLAG_video && media_type == MediaType::VIDEO)
      continue;
    if (!FLAG_data && media_type == MediaType::DATA)
      continue;
    if (strlen(FLAG_ssrc) > 0 && stream.ssrc != ssrc_filter)
      continue;
    auto rtp_view = absl::make_unique<
        webrtc::ProcessableEventList<webrtc::LoggedRtpPacketIncoming>>(
        stream.incoming_packets.begin(), stream.incoming_packets.end(),
        handle_rtp);
    event_processor.AddEvents(std::move(rtp_view));
  }
  auto rtcp_view = absl::make_unique<
      webrtc::ProcessableEventList<webrtc::LoggedRtcpPacketIncoming>>(
      parsed_stream.incoming_rtcp_packets().begin(),
      parsed_stream.incoming_rtcp_packets().end(), handle_rtcp);
  event_processor.AddEvents(std::move(rtcp_view));

  event_processor.ProcessEventsInOrder();

  std::cout << "Wrote " << rtp_counter << (header_only ? " header-only" : "")
            << " RTP packets and " << rtcp_counter << " RTCP packets to the "
            << "output file." << std::endl;
  return 0;
}
