/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/videocodec_test_stats.h"

#include "absl/strings/str_cat.h"

namespace webrtc {
namespace test {

std::string VideoCodecTestStats::FrameStatistics::ToString() const {
  return absl::StrCat(
      "frame_number ", frame_number, " decoded_width ", decoded_width,
      " decoded_height ", decoded_height, " spatial_idx ", spatial_idx,
      " temporal_idx ", temporal_idx, " inter_layer_predicted ",
      inter_layer_predicted, " non_ref_for_inter_layer_pred ",
      non_ref_for_inter_layer_pred, " frame_type ", frame_type,
      " length_bytes ", length_bytes, " qp ", qp, " psnr ", psnr, " psnr_y ",
      psnr_y, " psnr_u ", psnr_u, " psnr_v ", psnr_v, " ssim ", ssim,
      " encode_time_us ", encode_time_us, " decode_time_us ", decode_time_us,
      " rtp_timestamp ", rtp_timestamp, " target_bitrate_kbps ",
      target_bitrate_kbps);
}

VideoCodecTestStats::VideoStatistics::VideoStatistics() = default;
VideoCodecTestStats::VideoStatistics::VideoStatistics(const VideoStatistics&) =
    default;

std::string VideoCodecTestStats::VideoStatistics::ToString(
    std::string prefix) const {
  return absl::StrCat(
      prefix, "target_bitrate_kbps: ", target_bitrate_kbps, "\n", prefix,
      "input_framerate_fps: ", input_framerate_fps, "\n", prefix,
      "spatial_idx: ", spatial_idx, "\n", prefix,
      "temporal_idx: ", temporal_idx, "\n", prefix, "width: ", width, "\n",
      prefix, "height: ", height, "\n", prefix, "length_bytes: ", length_bytes,
      "\n", prefix, "bitrate_kbps: ", bitrate_kbps, "\n", prefix,
      "framerate_fps: ", framerate_fps, "\n", prefix,
      "enc_speed_fps: ", enc_speed_fps, "\n", prefix,
      "dec_speed_fps: ", dec_speed_fps, "\n", prefix,
      "avg_delay_sec: ", avg_delay_sec, "\n", prefix,
      "max_key_frame_delay_sec: ", max_key_frame_delay_sec, "\n", prefix,
      "max_delta_frame_delay_sec: ", max_delta_frame_delay_sec, "\n", prefix,
      "time_to_reach_target_bitrate_sec: ", time_to_reach_target_bitrate_sec,
      "\n", prefix, "avg_key_frame_size_bytes: ", avg_key_frame_size_bytes,
      "\n", prefix, "avg_delta_frame_size_bytes: ", avg_delta_frame_size_bytes,
      "\n", prefix, "avg_qp: ", avg_qp, "\n", prefix, "avg_psnr: ", avg_psnr,
      "\n", prefix, "min_psnr: ", min_psnr, "\n", prefix,
      "avg_ssim: ", avg_ssim, "\n", prefix, "min_ssim: ", min_ssim, "\n",
      prefix, "num_input_frames: ", num_input_frames, "\n", prefix,
      "num_encoded_frames: ", num_encoded_frames, "\n", prefix,
      "num_decoded_frames: ", num_decoded_frames, "\n", prefix,
      "num_dropped_frames: ", num_input_frames - num_encoded_frames, "\n",
      prefix, "num_key_frames: ", num_key_frames, "\n", prefix,
      "num_spatial_resizes: ", num_spatial_resizes, "\n", prefix,
      "max_nalu_size_bytes: ", max_nalu_size_bytes);
}

VideoCodecTestStats::FrameStatistics::FrameStatistics(size_t frame_number,
                                                      size_t rtp_timestamp)
    : frame_number(frame_number), rtp_timestamp(rtp_timestamp) {}

VideoCodecTestStats::FrameStatistics::FrameStatistics(
    const FrameStatistics& rhs) = default;

}  // namespace test
}  // namespace webrtc
