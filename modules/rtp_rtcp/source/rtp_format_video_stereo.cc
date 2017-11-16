/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_format_video_stereo.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
constexpr size_t kStereoHeaderMarkerLength = 1;
constexpr size_t kStereoHeaderLength = sizeof(RTPVideoHeaderStereo);
constexpr uint8_t kFirstPacketBit = 0x02;
}  // unnamed namespace

RtpPacketizerStereo::RtpPacketizerStereo(const RTPVideoHeaderStereo& header,
                                         FrameType frame_type,
                                         size_t max_payload_len,
                                         size_t last_packet_reduction_len)
    : header_(header),
      max_payload_len_(max_payload_len - kStereoHeaderMarkerLength -
                       kStereoHeaderLength),
      last_packet_reduction_len_(last_packet_reduction_len),
      packetizer_(frame_type, max_payload_len_, last_packet_reduction_len_) {}

RtpPacketizerStereo::~RtpPacketizerStereo() {}

size_t RtpPacketizerStereo::SetPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation) {
  header_marker_ = kFirstPacketBit;
  return packetizer_.SetPayloadData(payload_data, payload_size, fragmentation);
}

bool RtpPacketizerStereo::NextPacket(RtpPacketToSend* packet) {
  RTC_DCHECK(packet);
  if (!packetizer_.NextPacket(packet))
    return false;

  const bool first_packet = header_marker_ == kFirstPacketBit;
  size_t header_length = first_packet
                             ? kStereoHeaderMarkerLength + kStereoHeaderLength
                             : kStereoHeaderMarkerLength;

  std::unique_ptr<RtpPacketToSend> copied_packet(new RtpPacketToSend(*packet));
  uint8_t* wrapped_payload =
      packet->AllocatePayload(header_length + packet->payload_size());
  RTC_DCHECK(wrapped_payload);
  wrapped_payload[0] = header_marker_;
  header_marker_ &= ~kFirstPacketBit;
  if (first_packet) {
    memcpy(&wrapped_payload[kStereoHeaderMarkerLength], &header_,
           kStereoHeaderLength);
  }
  auto payload = copied_packet->payload();
  memcpy(&wrapped_payload[header_length], payload.data(), payload.size());
  return true;
}

std::string RtpPacketizerStereo::ToString() {
  return "RtpPacketizerStereo";
}

RtpDepacketizerStereo::~RtpDepacketizerStereo() {}

bool RtpDepacketizerStereo::Parse(ParsedPayload* parsed_payload,
                                  const uint8_t* payload_data,
                                  size_t payload_data_length) {
  RTC_DCHECK(parsed_payload != NULL);
  if (payload_data_length == 0) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return false;
  }

  uint8_t marker_header = *payload_data++;
  --payload_data_length;
  const bool first_packet = (marker_header & kFirstPacketBit) != 0;

  if (first_packet) {
    memcpy(&parsed_payload->type.Video.codecHeader.stereo, payload_data,
           kStereoHeaderLength);
    payload_data += kStereoHeaderLength;
    payload_data_length -= kStereoHeaderLength;
  }
  const bool rv =
      depacketizer_.Parse(parsed_payload, payload_data, payload_data_length);
  RTC_DCHECK(rv);
  RTC_DCHECK_EQ(parsed_payload->type.Video.is_first_packet_in_frame,
                first_packet);
  parsed_payload->type.Video.codec = kRtpVideoStereo;
  return rv;
}
}  // namespace webrtc
