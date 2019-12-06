/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_depacketizer_vp8.h"

#include <memory>

#include "modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::make_tuple;

constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
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
// T/K: |TID:Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |     Size1     |
//      +-+-+-+-+-+-+-+-+
//      |     Size2     |
//      +-+-+-+-+-+-+-+-+
//      | Bytes 4..N of |
//      | VP8 payload   |
//      :               :
//      +-+-+-+-+-+-+-+-+
//      | OPTIONAL RTP  |
//      | padding       |
//      :               :
//      +-+-+-+-+-+-+-+-+
void VerifyBasicHeader(const RTPVideoHeader& header,
                       bool N,
                       bool S,
                       int part_id) {
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header.video_type_header);
  EXPECT_EQ(N, vp8_header.nonReference);
  EXPECT_EQ(S, vp8_header.beginningOfPartition);
  EXPECT_EQ(part_id, vp8_header.partitionId);
}

void VerifyExtensions(const RTPVideoHeader& header,
                      int16_t picture_id,   /* I */
                      int16_t tl0_pic_idx,  /* L */
                      uint8_t temporal_idx, /* T */
                      int key_idx /* K */) {
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header.video_type_header);
  EXPECT_EQ(picture_id, vp8_header.pictureId);
  EXPECT_EQ(tl0_pic_idx, vp8_header.tl0PicIdx);
  EXPECT_EQ(temporal_idx, vp8_header.temporalIdx);
  EXPECT_EQ(key_idx, vp8_header.keyIdx);
}

TEST(RtpVideoDepacketizerVp8Test, BasicHeader) {
  uint8_t packet[4] = {0};
  packet[0] = 0x14;  // Binary 0001 0100; S = 1, PartID = 4.
  packet[1] = 0x01;  // P frame.

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &video_header);

  EXPECT_EQ(offset, 1u);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(video_header.codec, kVideoCodecVP8);
  VerifyBasicHeader(video_header, 0, 1, 4);
  VerifyExtensions(video_header, kNoPictureId, kNoTl0PicIdx, kNoTemporalIdx,
                   kNoKeyIdx);
}

TEST(RtpVideoDepacketizerVp8Test, OneBytePictureID) {
  const uint16_t kPictureId = 17;
  uint8_t packet[10] = {0};
  packet[0] = 0xA0;
  packet[1] = 0x80;
  packet[2] = kPictureId;

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &video_header);

  EXPECT_EQ(offset, 3u);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(video_header.codec, kVideoCodecVP8);
  VerifyBasicHeader(video_header, 1, 0, 0);
  VerifyExtensions(video_header, kPictureId, kNoTl0PicIdx, kNoTemporalIdx,
                   kNoKeyIdx);
}

TEST(RtpVideoDepacketizerVp8Test, TwoBytePictureID) {
  const uint16_t kPictureId = 17;
  uint8_t packet[10] = {0};
  packet[0] = 0xA0;
  packet[1] = 0x80;
  packet[2] = 0x80 | kPictureId;
  packet[3] = kPictureId;

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &video_header);

  EXPECT_EQ(offset, 4u);
  VerifyBasicHeader(video_header, 1, 0, 0);
  VerifyExtensions(video_header, (kPictureId << 8) + kPictureId, kNoTl0PicIdx,
                   kNoTemporalIdx, kNoKeyIdx);
}

TEST(RtpVideoDepacketizerVp8Test, Tl0PicIdx) {
  const uint8_t kTl0PicIdx = 17;
  uint8_t packet[13] = {0};
  packet[0] = 0x90;
  packet[1] = 0x40;
  packet[2] = kTl0PicIdx;

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &video_header);

  EXPECT_EQ(offset, 3u);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(video_header.codec, kVideoCodecVP8);
  VerifyBasicHeader(video_header, 0, 1, 0);
  VerifyExtensions(video_header, kNoPictureId, kTl0PicIdx, kNoTemporalIdx,
                   kNoKeyIdx);
}

TEST(RtpVideoDepacketizerVp8Test, TIDAndLayerSync) {
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x20;
  packet[2] = 0x80;  // TID(2) + LayerSync(false)

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &video_header);

  EXPECT_EQ(offset, 3u);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(video_header.codec, kVideoCodecVP8);
  VerifyBasicHeader(video_header, 0, 0, 8);
  VerifyExtensions(video_header, kNoPictureId, kNoTl0PicIdx, 2, kNoKeyIdx);
  EXPECT_FALSE(
      absl::get<RTPVideoHeaderVP8>(video_header.video_type_header).layerSync);
}

TEST(RtpVideoDepacketizerVp8Test, KeyIdx) {
  const uint8_t kKeyIdx = 17;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x10;  // K = 1.
  packet[2] = kKeyIdx;

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &video_header);

  EXPECT_EQ(offset, 3u);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(video_header.codec, kVideoCodecVP8);
  VerifyBasicHeader(video_header, 0, 0, 8);
  VerifyExtensions(video_header, kNoPictureId, kNoTl0PicIdx, kNoTemporalIdx,
                   kKeyIdx);
}

TEST(RtpVideoDepacketizerVp8Test, MultipleExtensions) {
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x80 | 0x40 | 0x20 | 0x10;
  packet[2] = 0x80 | 17;           // PictureID, high 7 bits.
  packet[3] = 17;                  // PictureID, low 8 bits.
  packet[4] = 42;                  // Tl0PicIdx.
  packet[5] = 0x40 | 0x20 | 0x11;  // TID(1) + LayerSync(true) + KEYIDX(17).

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &video_header);

  EXPECT_EQ(offset, 6u);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(video_header.codec, kVideoCodecVP8);
  VerifyBasicHeader(video_header, 0, 0, 8);
  VerifyExtensions(video_header, (17 << 8) + 17, 42, 1, 17);
}

TEST(RtpVideoDepacketizerVp8Test, TooShortHeader) {
  uint8_t packet[4] = {0};
  packet[0] = 0x88;
  packet[1] = 0x80 | 0x40 | 0x20 | 0x10;  // All extensions are enabled...
  packet[2] = 0x80 | 17;  // ... but only 2 bytes PictureID is provided.
  packet[3] = 17;         // PictureID, low 8 bits.
  RtpDepacketizer::ParsedPayload payload;

  RTPVideoHeader unused;
  EXPECT_EQ(RtpVideoDepacketizerVp8::ParseRtpPayload(packet, &unused), 0u);
}

TEST(RtpVideoDepacketizerVp8Test, WithPacketizer) {
  uint8_t data[10] = {0};
  RtpPacketToSend packet(kNoExtensions);
  RTPVideoHeaderVP8 input_header;
  input_header.nonReference = true;
  input_header.pictureId = 300;
  input_header.temporalIdx = 1;
  input_header.layerSync = false;
  input_header.tl0PicIdx = kNoTl0PicIdx;  // Disable.
  input_header.keyIdx = 31;
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 20;
  RtpPacketizerVp8 packetizer(data, limits, input_header);
  EXPECT_EQ(packetizer.NumPackets(), 1u);
  ASSERT_TRUE(packetizer.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());

  RTPVideoHeader video_header;
  size_t offset =
      RtpVideoDepacketizerVp8::ParseRtpPayload(packet.payload(), &video_header);

  EXPECT_EQ(offset, 5u);
  EXPECT_EQ(video_header.frame_type, VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(video_header.codec, kVideoCodecVP8);
  VerifyBasicHeader(video_header, 1, 1, 0);
  VerifyExtensions(video_header, input_header.pictureId, input_header.tl0PicIdx,
                   input_header.temporalIdx, input_header.keyIdx);
  EXPECT_EQ(
      absl::get<RTPVideoHeaderVP8>(video_header.video_type_header).layerSync,
      input_header.layerSync);
}

TEST(RtpVideoDepacketizerVp8Test, ReferencesInputCopyOnWriteBuffer) {
  constexpr size_t kHeaderSize = 6;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x80 | 0x40 | 0x20 | 0x10;
  packet[2] = 0x80 | 17;           // PictureID, high 7 bits.
  packet[3] = 17;                  // PictureID, low 8 bits.
  packet[4] = 42;                  // Tl0PicIdx.
  packet[5] = 0x40 | 0x20 | 0x11;  // TID(1) + LayerSync(true) + KEYIDX(17).
  rtc::CopyOnWriteBuffer rtp_payload(packet);

  RtpVideoDepacketizerVp8 depacketizer;
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);

  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->video_payload.size(), rtp_payload.size() - kHeaderSize);
  // Compare pointers to check there was no copy on write buffer unsharing.
  EXPECT_EQ(parsed->video_payload.cdata(), rtp_payload.cdata() + kHeaderSize);
}

TEST(RtpVideoDepacketizerVp8Test, FailsOnEmptyPayload) {
  rtc::ArrayView<const uint8_t> empty;
  RTPVideoHeader video_header;
  EXPECT_EQ(RtpVideoDepacketizerVp8::ParseRtpPayload(empty, &video_header), 0u);
}

}  // namespace
}  // namespace webrtc
