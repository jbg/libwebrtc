/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_depacketizer_vp8.h"

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
constexpr size_t kFailedToParse = 0;
//
// VP8 format:
//
// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |   PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID|Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header (considered part of the actual payload, sent to decoder)
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |      ...      |
//      +               +
size_t ParseVP8Extension(RTPVideoHeaderVP8* vp8,
                         rtc::ArrayView<const uint8_t> data) {
  RTC_DCHECK(!data.empty());
  // Optional X field is present.
  bool has_picture_id = (data[0] & 0x80);   // I bit
  bool has_tl0_pic_idx = (data[0] & 0x40);  // L bit
  bool has_tid = (data[0] & 0x20);          // T bit
  bool has_key_idx = (data[0] & 0x10);      // K bit
  size_t parsed_bytes = 1;

  if (has_picture_id) {
    if (data.size() == parsed_bytes)
      return kFailedToParse;

    vp8->pictureId = data[parsed_bytes] & 0x7F;
    if (data[parsed_bytes] & 0x80) {
      parsed_bytes++;
      if (data.size() == parsed_bytes)
        return kFailedToParse;
      // PictureId is 15 bits
      vp8->pictureId = (vp8->pictureId << 8) | data[parsed_bytes];
    }
    parsed_bytes++;
  }

  if (has_tl0_pic_idx) {
    if (data.size() == parsed_bytes)
      return kFailedToParse;

    vp8->tl0PicIdx = data[parsed_bytes];
    parsed_bytes++;
  }

  if (has_tid || has_key_idx) {
    if (data.size() == parsed_bytes)
      return kFailedToParse;

    if (has_tid) {
      vp8->temporalIdx = ((data[parsed_bytes] >> 6) & 0x03);
      vp8->layerSync = (data[parsed_bytes] & 0x20) != 0;  // Y bit
    }
    if (has_key_idx) {
      vp8->keyIdx = (data[parsed_bytes] & 0x1F);
    }
    parsed_bytes++;
  }
  return parsed_bytes;
}

}  // namespace

absl::optional<VideoRtpDepacketizer::ParsedRtpPayload>
RtpVideoDepacketizerVp8::Parse(rtc::CopyOnWriteBuffer rtp_payload) {
  rtc::ArrayView<const uint8_t> payload(rtp_payload.cdata(),
                                        rtp_payload.size());
  absl::optional<ParsedRtpPayload> parsed(absl::in_place);
  size_t offset = ParseRtpPayload(payload, &parsed->video_header);
  if (offset == kFailedToParse)
    return absl::nullopt;

  RTC_DCHECK_LT(offset, rtp_payload.size());
  parsed->video_payload =
      rtp_payload.Slice(offset, rtp_payload.size() - offset);
  return parsed;
}

size_t RtpVideoDepacketizerVp8::ParseRtpPayload(
    rtc::ArrayView<const uint8_t> payload,
    RTPVideoHeader* video_header) {
  if (payload.empty()) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return kFailedToParse;
  }

  // Parse mandatory first byte of payload descriptor.
  bool extension = (payload[0] & 0x80);               // X bit
  bool beginning_of_partition = (payload[0] & 0x10);  // S bit
  int partition_id = payload[0] & 0x0F;               // PartID field

  video_header->is_first_packet_in_frame =
      beginning_of_partition && (partition_id == 0);
  video_header->simulcastIdx = 0;
  video_header->codec = kVideoCodecVP8;
  auto& vp8_header =
      video_header->video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.nonReference = (payload[0] & 0x20) != 0;  // N bit
  vp8_header.partitionId = partition_id;
  vp8_header.beginningOfPartition = beginning_of_partition;
  vp8_header.pictureId = kNoPictureId;
  vp8_header.tl0PicIdx = kNoTl0PicIdx;
  vp8_header.temporalIdx = kNoTemporalIdx;
  vp8_header.layerSync = false;
  vp8_header.keyIdx = kNoKeyIdx;

  if (partition_id > 8) {
    // Weak check for corrupt payload_data: PartID MUST NOT be larger than 8.
    return kFailedToParse;
  }

  size_t offset = 1;
  if (payload.size() <= offset) {
    RTC_LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
    return kFailedToParse;
  }

  if (extension) {
    size_t parsed_bytes =
        ParseVP8Extension(&vp8_header, payload.subview(offset));
    if (parsed_bytes == kFailedToParse)
      return kFailedToParse;

    offset += parsed_bytes;
    if (payload.size() <= offset) {
      RTC_LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
      return kFailedToParse;
    }
  }

  // Read P bit from payload header (only at beginning of first partition).
  if (beginning_of_partition && partition_id == 0 &&
      (payload[offset] & 0x01) == 0) {
    video_header->frame_type = VideoFrameType::kVideoFrameKey;

    rtc::ArrayView<const uint8_t> frame = payload.subview(offset);
    if (frame.size() < 10) {
      // For an I-frame we should always have the uncompressed VP8 header
      // in the beginning of the partition.
      return kFailedToParse;
    }
    video_header->width = ((frame[7] << 8) + frame[6]) & 0x3FFF;
    video_header->height = ((frame[9] << 8) + frame[8]) & 0x3FFF;
  } else {
    video_header->frame_type = VideoFrameType::kVideoFrameDelta;

    video_header->width = 0;
    video_header->height = 0;
  }

  return offset;
}
}  // namespace webrtc
