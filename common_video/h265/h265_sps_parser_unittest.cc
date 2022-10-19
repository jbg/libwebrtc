/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_sps_parser.h"

#include "common_video/h265/h265_common.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {

// Example SPS can be generated with ffmpeg. Here's an example set of commands,
// runnable on OS X:
// 1) Generate a video, from the camera:
// ffmpeg -f avfoundation -r 30 -i "0" -c:v libx265 -s 1280x720 camera.mov
//
// 2) Crop the video to expected size(for example, 640x260 which will crop
// from 640x264):
// ffmpeg -i camera.mov -filter:v crop=640:260:200:200 -c:v libx265 cropped.mov
//
// 3) Get just the H.265 bitstream in AnnexB:
// ffmpeg -i cropped.mov -vcodec copy -vbsf hevc_mp4toannexb -an out.hevc
//
// 4) Open out.hevc and find the SPS, generally everything between the second
// and third start codes (0 0 0 1 or 0 0 1). The first two bytes should be 0x42
// and 0x01, which should be stripped out before being passed to the parser.

static const size_t kSpsBufferMaxSize = 256;

// Generates a fake SPS with basically everything empty but the width/height.
// Pass in a buffer of at least kSpsBufferMaxSize.
// The fake SPS that this generates also always has at least one emulation byte
// at offset 2, since the first two bytes are always 0, and has a 0x3 as the
// level_idc, to make sure the parser doesn't eat all 0x3 bytes.
void GenerateFakeSps(uint16_t width,
                     uint16_t height,
                     int id,
                     uint32_t max_num_sublayer_minus1,
                     rtc::Buffer* out_buffer) {
  uint8_t rbsp[kSpsBufferMaxSize] = {0};
  rtc::BitBufferWriter writer(rbsp, kSpsBufferMaxSize);
  // sps_video_parameter_set_id
  writer.WriteBits(0, 4);
  // sps_max_sub_layers_minus1
  writer.WriteBits(0, 3);
  // sps_temporal_id_nesting_flag
  writer.WriteBits(1, 1);
  // profile_tier_level(profilePresentFlag=1, maxNumSublayersMinus1=0)
  // profile-space=0, tier=0, profile-idc=1
  writer.WriteBits(0, 2);
  writer.WriteBits(0, 1);
  writer.WriteBits(1, 5);
  // general_prfile_compatibility_flag[32]
  writer.WriteBits(0, 32);
  // general_progressive_source_flag
  writer.WriteBits(1, 1);
  // general_interlace_source_flag
  writer.WriteBits(0, 1);
  // general_non_packed_constraint_flag
  writer.WriteBits(0, 1);
  // general_frame_only_constraint_flag
  writer.WriteBits(1, 1);
  // general_reserved_zero_7bits
  writer.WriteBits(0, 7);
  // general_one_picture_only_flag
  writer.WriteBits(0, 1);
  // general_reserved_zero_35bits
  writer.WriteBits(0, 35);
  // general_inbld_flag
  writer.WriteBits(0, 1);
  // general_level_idc
  writer.WriteBits(93, 8);
  // seq_parameter_set_id
  writer.WriteExponentialGolomb(id);
  // chroma_format_idc
  writer.WriteExponentialGolomb(1);
  // pic_width_in_luma_samples
  writer.WriteExponentialGolomb(width);
  // pic_height_in_luma_samples
  writer.WriteExponentialGolomb(height);
  // conformance_window_flag
  writer.WriteBits(0, 1);
  // bit_depth_luma_minus8
  writer.WriteExponentialGolomb(0);
  // bit_depth_chroma_minus8
  writer.WriteExponentialGolomb(0);
  // log2_max_pic_order_cnt_lsb_minus4
  writer.WriteExponentialGolomb(4);
  // sps_sub_layer_ordering_info_present_flag
  writer.WriteBits(0, 1);
  // log2_min_luma_coding_block_size_minus3
  writer.WriteExponentialGolomb(0);
  // log2_diff_max_min_luma_coding_block_size
  writer.WriteExponentialGolomb(3);
  // log2_min_luma_transform_block_size_minus2
  writer.WriteExponentialGolomb(0);
  // log2_diff_max_min_luma_transform_block_size
  writer.WriteExponentialGolomb(3);
  // max_transform_hierarchy_depth_inter
  writer.WriteExponentialGolomb(0);
  // max_transform_hierarchy_depth_intra
  writer.WriteExponentialGolomb(0);
  // scaling_list_enabled_flag
  writer.WriteBits(0, 1);
  // apm_enabled_flag
  writer.WriteBits(0, 1);
  // sample_adaptive_offset_enabled_flag
  writer.WriteBits(1, 1);
  // pcm_enabled_flag
  writer.WriteBits(0, 1);
  // num_short_term_ref_pic_sets
  writer.WriteExponentialGolomb(0);
  // long_term_ref_pics_present_flag
  writer.WriteBits(0, 1);
  // sps_temproal_mvp_enabled_flag
  writer.WriteBits(1, 1);
  // strong_intra_smoothing_enabled_flag
  writer.WriteBits(1, 1);
  // vui_parameters_present_flag
  writer.WriteBits(0, 1);
  // sps_extension_present_flag
  writer.WriteBits(0, 1);

  // Get the number of bytes written (including the last partial byte).
  size_t byte_count, bit_offset;
  writer.GetCurrentOffset(&byte_count, &bit_offset);
  if (bit_offset > 0) {
    byte_count++;
  }

  out_buffer->Clear();
  H265::WriteRbsp(rbsp, byte_count, out_buffer);
}

class H265SpsParserTest : public ::testing::Test {
 public:
  H265SpsParserTest() {}
  ~H265SpsParserTest() override {}

  absl::optional<SpsParser::SpsState> sps_;
};

TEST_F(H265SpsParserTest, TestSampleSPSHdLandscape) {
  // SPS for a 1280x720 camera capture from ffmpeg on osx. Contains
  // emulation bytes but no cropping.
  const uint8_t buffer[] = {0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9D,
                            0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x5D, 0xB0,
                            0x02, 0x80, 0x80, 0x2D, 0x16, 0x59, 0x59, 0xA4,
                            0x93, 0x2B, 0x9A, 0x02, 0x00, 0x00, 0x03, 0x00,
                            0x02, 0x00, 0x00, 0x03, 0x00, 0x3C, 0x10};
  EXPECT_TRUE(
      static_cast<bool>(sps_ = SpsParser::ParseSps(buffer, arraysize(buffer))));
  EXPECT_EQ(1280u, sps_->width);
  EXPECT_EQ(720u, sps_->height);
}

TEST_F(H265SpsParserTest, TestSampleSPSVerticalCropLandscape) {
  // SPS for a 640x260 camera capture from ffmpeg on osx,. Contains emulation
  // bytes and vertical cropping (crop from 640x264).
  const uint8_t buffer[] = {0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9D,
                            0x08, 0x00, 0x00, 0x30, 0x00, 0x00, 0x3F, 0xB0,
                            0x05, 0x02, 0x01, 0x09, 0xF2, 0xE5, 0x95, 0x9A,
                            0x49, 0x32, 0xB9, 0xA0, 0x20, 0x00, 0x00, 0x03,
                            0x00, 0x20, 0x00, 0x00, 0x03, 0x03, 0xC1};
  EXPECT_TRUE(
      static_cast<bool>(sps_ = SpsParser::ParseSps(buffer, arraysize(buffer))));
  EXPECT_EQ(640u, sps_->width);
  EXPECT_EQ(260u, sps_->height);
}

TEST_F(H265SpsParserTest, TestSampleSPSHorizontalAndVerticalCrop) {
  // SPS for a 260x260 camera capture from ffmpeg on osx. Contains emulation
  // bytes. Horizontal and veritcal crop (Crop from 264x264).
  const uint8_t buffer[] = {0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9D,
                            0x08, 0x00, 0x00, 0x30, 0x00, 0x00, 0x3C, 0xB0,
                            0x08, 0x48, 0x04, 0x27, 0x72, 0xE5, 0x95, 0x9A,
                            0x49, 0x32, 0xb9, 0xA0, 0x20, 0x00, 0x00, 0x03,
                            0x00, 0x20, 0x00, 0x00, 0x03, 0x03, 0xC1};
  EXPECT_TRUE(
      static_cast<bool>(sps_ = SpsParser::ParseSps(buffer, arraysize(buffer))));
  EXPECT_EQ(260u, sps_->width);
  EXPECT_EQ(260u, sps_->height);
}

TEST_F(H265SpsParserTest, TestSyntheticSPSQvgaLandscape) {
  rtc::Buffer buffer;
  GenerateFakeSps(320u, 180u, 1, 0, 0, &buffer);
  EXPECT_TRUE(static_cast<bool>(
      sps_ = SpsParser::ParseSps(buffer.data(), buffer.size())));
  EXPECT_EQ(320u, sps_->width);
  EXPECT_EQ(180u, sps_->height);
  EXPECT_EQ(1u, sps_->id);
}

TEST_F(H265SpsParserTest, TestSyntheticSPSWeirdResolution) {
  rtc::Buffer buffer;
  GenerateFakeSps(156u, 122u, 2, 0, 0, &buffer);
  EXPECT_TRUE(static_cast<bool>(
      sps_ = SpsParser::ParseSps(buffer.data(), buffer.size())));
  EXPECT_EQ(156u, sps_->width);
  EXPECT_EQ(122u, sps_->height);
  EXPECT_EQ(2u, sps_->id);
}

TEST_F(H265SpsParserTest, TestLog2MaxFrameNumMinus4) {
  rtc::Buffer buffer;
  GenerateFakeSps(320u, 180u, 1, 0, 0, &buffer);
  EXPECT_TRUE(static_cast<bool>(
      sps_ = SpsParser::ParseSps(buffer.data(), buffer.size())));
  EXPECT_EQ(320u, sps_->width);
  EXPECT_EQ(180u, sps_->height);
  EXPECT_EQ(1u, sps_->id);
  EXPECT_EQ(4u, sps_->log2_max_frame_num);

  GenerateFakeSps(320u, 180u, 1, 28, 0, &buffer);
  EXPECT_TRUE(static_cast<bool>(
      sps_ = SpsParser::ParseSps(buffer.data(), buffer.size())));
  EXPECT_EQ(320u, sps_->width);
  EXPECT_EQ(180u, sps_->height);
  EXPECT_EQ(1u, sps_->id);
  EXPECT_EQ(32u, sps_->log2_max_frame_num);

  GenerateFakeSps(320u, 180u, 1, 29, 0, &buffer);
  EXPECT_FALSE(SpsParser::ParseSps(buffer.data(), buffer.size()));
}

TEST_F(H265SpsParserTest, TestLog2MaxPicOrderCntMinus4) {
  rtc::Buffer buffer;
  GenerateFakeSps(320u, 180u, 1, 0, 0, &buffer);
  EXPECT_TRUE(static_cast<bool>(
      sps_ = SpsParser::ParseSps(buffer.data(), buffer.size())));
  EXPECT_EQ(320u, sps_->width);
  EXPECT_EQ(180u, sps_->height);
  EXPECT_EQ(1u, sps_->id);
  EXPECT_EQ(4u, sps_->log2_max_pic_order_cnt_lsb);

  GenerateFakeSps(320u, 180u, 1, 0, 28, &buffer);
  EXPECT_TRUE(static_cast<bool>(
      sps_ = SpsParser::ParseSps(buffer.data(), buffer.size())));
  EXPECT_EQ(320u, sps_->width);
  EXPECT_EQ(180u, sps_->height);
  EXPECT_EQ(1u, sps_->id);
  EXPECT_EQ(32u, sps_->log2_max_pic_order_cnt_lsb);

  GenerateFakeSps(320u, 180u, 1, 0, 29, &buffer);
  EXPECT_FALSE(SpsParser::ParseSps(buffer.data(), buffer.size()));
}

}  // namespace webrtc
