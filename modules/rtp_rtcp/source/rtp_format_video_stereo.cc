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
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format_video_stereo.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
// Write the Stereo header descriptor.
//       0 1 2 3 4 5 6 7 8
//      +-+-+-+-+-+-+-+-+-+
//      | VideoCodecType  | (optional)
//      +-+-+-+-+-+-+-+-+-+
//      |   frame_index   | (optional)
//      +-+-+-+-+-+-+-+-+-+
//      |   frame_count   | (optional)
//      +-+-+-+-+-+-+-+-+-+
//      |picture_index(64)| (optional)
//      +-+-+-+-+-+-+-+-+-+
//      |  HeaderMarker   | (mandatory)
//      +-+-+-+-+-+-+-+-+-+
constexpr size_t kStereoHeaderLength =
    sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint64_t);

constexpr size_t kHeaderMarkerLength = 1;
constexpr uint8_t kHeaderMarkerBit = 0x02;
}  // namespace

RtpPacketizerStereo::RtpPacketizerStereo(const RTPVideoHeaderStereo& header,
                                         FrameType frame_type,
                                         size_t max_payload_len,
                                         size_t last_packet_reduction_len)
    : header_(header),
      max_payload_len_(max_payload_len - kHeaderMarkerLength),
      last_packet_reduction_len_(last_packet_reduction_len +
                                 kStereoHeaderLength),
      packetizer_(frame_type, max_payload_len_, last_packet_reduction_len_) {}

RtpPacketizerStereo::~RtpPacketizerStereo() {}

size_t RtpPacketizerStereo::SetPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation) {
  return packetizer_.SetPayloadData(payload_data, payload_size, fragmentation);
}

bool RtpPacketizerStereo::NextPacket(RtpPacketToSend* packet) {
  if (max_payload_len_ <= last_packet_reduction_len_) {
    RTC_LOG(LS_ERROR) << "Payload length not large enough.";
    return false;
  }

  RTC_DCHECK(packet);
  if (!packetizer_.NextPacket(packet))
    return false;

  const bool last_packet = packet->Marker();
  const size_t header_length = last_packet
                             ? kHeaderMarkerLength + kStereoHeaderLength
                             : kHeaderMarkerLength;

  const uint8_t* payload_ptr = packet->payload().data();
  const size_t payload_size = packet->payload_size();
  uint8_t* padded_payload_ptr =
      packet->SetPayloadSize(header_length + packet->payload_size());
  if (payload_ptr != padded_payload_ptr) {
    RTC_LOG(LS_ERROR) << "Error extending payload memory allocation.";
    return false;
  }

  padded_payload_ptr += payload_size;
  if (last_packet) {
    ByteWriter<uint8_t>::WriteBigEndian(
        padded_payload_ptr,
        static_cast<uint8_t>(header_.associated_codec_type));
    padded_payload_ptr += sizeof(uint8_t);
    ByteWriter<uint8_t>::WriteBigEndian(padded_payload_ptr,
                                        header_.frame_index);
    padded_payload_ptr += sizeof(uint8_t);
    ByteWriter<uint8_t>::WriteBigEndian(padded_payload_ptr,
                                        header_.frame_count);
    padded_payload_ptr += sizeof(uint8_t);
    ByteWriter<uint64_t>::WriteBigEndian(padded_payload_ptr,
                                         header_.picture_index);
    padded_payload_ptr += sizeof(uint64_t);
    RTC_DCHECK_EQ(payload_size + kStereoHeaderLength,
                  padded_payload_ptr - payload_ptr);
  }
  padded_payload_ptr[0] = last_packet ? kHeaderMarkerBit : 0;
  return true;
}

std::string RtpPacketizerStereo::ToString() {
  return "RtpPacketizerStereo";
}

RtpDepacketizerStereo::~RtpDepacketizerStereo() {}

bool RtpDepacketizerStereo::Parse(ParsedPayload* parsed_payload,
                                  const uint8_t* payload_data,
                                  size_t payload_data_length) {
  RTC_DCHECK(parsed_payload);
  if (payload_data_length == 0) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return false;
  }

  uint8_t header_marker = payload_data[payload_data_length - 1];
  --payload_data_length;
  const bool last_packet = (header_marker & kHeaderMarkerBit) != 0;

  if (last_packet) {
    if (payload_data_length <= kStereoHeaderLength) {
      RTC_LOG(LS_ERROR) << "Payload not large enough.";
      return false;
    }
    size_t offset = payload_data_length - kStereoHeaderLength;
    uint8_t associated_codec_type =
        ByteReader<uint8_t>::ReadBigEndian(&payload_data[offset]);
    if (associated_codec_type < static_cast<uint8_t>(kRtpVideoVp8) ||
        associated_codec_type > static_cast<uint8_t>(kRtpVideoH264)) {
      RTC_LOG(LS_ERROR) << "Unexpected codec type.";
      return false;
    }
    parsed_payload->type.Video.codecHeader.stereo.associated_codec_type =
        static_cast<RtpVideoCodecTypes>(associated_codec_type);
    offset += sizeof(uint8_t);
    parsed_payload->type.Video.codecHeader.stereo.frame_index =
        ByteReader<uint8_t>::ReadBigEndian(&payload_data[offset]);
    offset += sizeof(uint8_t);
    parsed_payload->type.Video.codecHeader.stereo.frame_count =
        ByteReader<uint8_t>::ReadBigEndian(&payload_data[offset]);
    offset += sizeof(uint8_t);
    parsed_payload->type.Video.codecHeader.stereo.picture_index =
        ByteReader<uint64_t>::ReadBigEndian(&payload_data[offset]);
    RTC_DCHECK_EQ(payload_data_length,
                  offset + sizeof(uint64_t));
    payload_data_length -= kStereoHeaderLength;
  }
  const bool rv =
      depacketizer_.Parse(parsed_payload, payload_data, payload_data_length);
  if (!rv)
    return rv;
  parsed_payload->type.Video.codec = kRtpVideoStereo;
  return true;
}
}  // namespace webrtc
