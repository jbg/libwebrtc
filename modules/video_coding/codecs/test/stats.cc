/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/stats.h"

#include <algorithm>
#include <cmath>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"
#include "test/statistics.h"

namespace webrtc {
namespace test {

namespace {
const int kMaxBitrateMismatchPercent = 20;
}

std::string FrameStatistic::ToString() const {
  std::stringstream ss;
  ss << "frame_number " << frame_number;
  ss << " decoded_width " << decoded_width;
  ss << " decoded_height " << decoded_height;
  ss << " simulcast_svc_idx " << simulcast_svc_idx;
  ss << " temporal_layer_idx " << temporal_layer_idx;
  ss << " frame_type " << frame_type;
  ss << " encoded_frame_size_bytes " << encoded_frame_size_bytes;
  ss << " qp " << qp;
  ss << " psnr " << psnr;
  ss << " ssim " << ssim;
  ss << " encode_time_us " << encode_time_us;
  ss << " decode_time_us " << decode_time_us;
  ss << " rtp_timestamp " << rtp_timestamp;
  ss << " target_bitrate_kbps " << target_bitrate_kbps;
  return ss.str();
}

std::string VideoStatistic::ToString() const {
  std::stringstream ss;
  ss << "\n width " << width;
  ss << "\n height " << height;
  ss << "\n length_bytes " << length_bytes;
  ss << "\n bitrate_kbps " << bitrate_kbps;
  ss << "\n framerate_fps " << framerate_fps;
  ss << "\n encoding_speed_fps " << encoding_speed_fps;
  ss << "\n decoding_speed_fps " << decoding_speed_fps;
  ss << "\n avg_delay_sec " << avg_delay_sec;
  ss << "\n max_key_frame_delay_sec " << max_key_frame_delay_sec;
  ss << "\n max_delta_frame_delay_sec " << max_delta_frame_delay_sec;
  ss << "\n time_to_reach_target_bitrate_sec "
     << time_to_reach_target_bitrate_sec;
  ss << "\n avg_qp " << avg_qp;
  ss << "\n num_encoded_frames " << num_encoded_frames;
  ss << "\n num_decoded_frames " << num_decoded_frames;
  ss << "\n num_key_frames " << num_key_frames;
  ss << "\n num_spatial_resizes " << num_spatial_resizes;
  ss << "\n max_nalu_size_bytes " << max_nalu_size_bytes;
  ss << "\n avg_psnr " << avg_psnr;
  ss << "\n min_psnr " << min_psnr;
  ss << "\n avg_ssim " << avg_ssim;
  ss << "\n min_ssim " << min_ssim;
  return ss.str();
}

FrameStatistic* Stats::AddFrame(size_t timestamp, size_t layer_idx) {
  RTC_DCHECK(rtp_timestamp_to_frame_num_[layer_idx].find(timestamp) ==
             rtp_timestamp_to_frame_num_[layer_idx].end());
  const size_t frame_num = layer_idx_to_stats_[layer_idx].size();
  rtp_timestamp_to_frame_num_[layer_idx][timestamp] = frame_num;
  layer_idx_to_stats_[layer_idx].emplace_back(frame_num, timestamp);
  return &layer_idx_to_stats_[layer_idx].back();
}

FrameStatistic* Stats::GetFrame(size_t frame_num, size_t layer_idx) {
  RTC_CHECK_LT(frame_num, layer_idx_to_stats_[layer_idx].size());
  return &layer_idx_to_stats_[layer_idx][frame_num];
}

FrameStatistic* Stats::GetFrameWithTimestamp(size_t timestamp,
                                             size_t layer_idx) {
  RTC_DCHECK(rtp_timestamp_to_frame_num_[layer_idx].find(timestamp) !=
             rtp_timestamp_to_frame_num_[layer_idx].end());

  return GetFrame(rtp_timestamp_to_frame_num_[layer_idx][timestamp], layer_idx);
}

// TODO(ssilkin): Should target_kbps and input_fps be inferred from the stats?
VideoStatistic Stats::SliceAndCalcVideoStatistic(
    size_t first_frame_num,
    size_t last_frame_num,
    size_t spatial_layer_idx,
    size_t temporal_layer_idx,
    size_t target_kbps,
    float input_fps,
    bool aggregate_spatial_layers) {
  VideoStatistic video_stat;

  float buffer_level_bits = 0.0f;
  Statistics buffer_level_sec;

  Statistics key_frame_length_bytes;
  Statistics delta_frame_length_bytes;

  Statistics encoding_time_us;
  Statistics decoding_time_us;

  Statistics psnr;
  Statistics ssim;

  Statistics qp;

  size_t rtp_timestamp_first_frame = 0;
  size_t rtp_timestamp_prev_frame = 0;

  FrameStatistic last_successfully_decoded_frame(0, 0);

  size_t num_analyzed_frames = 0;

  for (size_t frame_num = first_frame_num; frame_num <= last_frame_num;
       ++frame_num) {
    FrameStatistic frame_stat =
        aggregate_spatial_layers
            ? AggregateFrameStatistic(frame_num, spatial_layer_idx)
            : *GetFrame(frame_num, spatial_layer_idx);

    if (frame_stat.temporal_layer_idx > temporal_layer_idx) {
      continue;
    }

    float time_since_first_frame_sec =
        1.0f * (frame_stat.rtp_timestamp - rtp_timestamp_first_frame) /
        kVideoPayloadTypeFrequency;
    float time_since_prev_frame_sec =
        1.0f * (frame_stat.rtp_timestamp - rtp_timestamp_prev_frame) /
        kVideoPayloadTypeFrequency;

    buffer_level_bits -= time_since_prev_frame_sec * 1000 * target_kbps;
    buffer_level_bits = std::max(0.0f, buffer_level_bits);
    buffer_level_bits += 8.0 * frame_stat.encoded_frame_size_bytes;
    buffer_level_sec.AddSample(buffer_level_bits / (1000 * target_kbps));

    video_stat.length_bytes += frame_stat.encoded_frame_size_bytes;

    if (frame_stat.encoding_successful) {
      ++video_stat.num_encoded_frames;

      if (frame_stat.frame_type == kVideoFrameKey) {
        key_frame_length_bytes.AddSample(frame_stat.encoded_frame_size_bytes);
        ++video_stat.num_key_frames;
      } else {
        delta_frame_length_bytes.AddSample(frame_stat.encoded_frame_size_bytes);
      }

      encoding_time_us.AddSample(frame_stat.encode_time_us);
      qp.AddSample(frame_stat.qp);

      video_stat.max_nalu_size_bytes = std::max(video_stat.max_nalu_size_bytes,
                                                frame_stat.max_nalu_size_bytes);
    }

    if (frame_stat.decoding_successful) {
      ++video_stat.num_decoded_frames;

      video_stat.width = frame_stat.decoded_width;
      video_stat.height = frame_stat.decoded_height;

      psnr.AddSample(frame_stat.psnr);
      ssim.AddSample(frame_stat.ssim);

      if (video_stat.num_decoded_frames > 1) {
        if (last_successfully_decoded_frame.decoded_width !=
                frame_stat.decoded_width ||
            last_successfully_decoded_frame.decoded_height !=
                frame_stat.decoded_height) {
          ++video_stat.num_spatial_resizes;
        }
      }

      decoding_time_us.AddSample(frame_stat.decode_time_us);
      last_successfully_decoded_frame = frame_stat;
    }

    if (num_analyzed_frames > 0) {
      if (video_stat.time_to_reach_target_bitrate_sec == 0.0f) {
        const float curr_kbps =
            8.0 * video_stat.length_bytes / 1000 / time_since_first_frame_sec;
        const float bitrate_mismatch_percent =
            100 * std::fabs(curr_kbps - target_kbps) / target_kbps;
        if (bitrate_mismatch_percent < kMaxBitrateMismatchPercent) {
          video_stat.time_to_reach_target_bitrate_sec =
              time_since_first_frame_sec;
        }
      }
    }

    rtp_timestamp_prev_frame = frame_stat.rtp_timestamp;
    if (num_analyzed_frames == 0) {
      rtp_timestamp_first_frame = frame_stat.rtp_timestamp;
    }

    ++num_analyzed_frames;
  }

  const size_t num_frames = last_frame_num - first_frame_num + 1;
  const float duration_sec = num_frames / input_fps;

  video_stat.bitrate_kbps =
      static_cast<size_t>(8 * video_stat.length_bytes / 1000 / duration_sec);
  video_stat.framerate_fps = video_stat.num_encoded_frames / duration_sec;

  video_stat.encoding_speed_fps = 1000000 / encoding_time_us.Mean();
  video_stat.decoding_speed_fps = 1000000 / decoding_time_us.Mean();

  video_stat.avg_delay_sec = buffer_level_sec.Mean();
  video_stat.max_key_frame_delay_sec =
      8 * key_frame_length_bytes.Max() / 1000 / target_kbps;
  video_stat.max_delta_frame_delay_sec =
      8 * delta_frame_length_bytes.Max() / 1000 / target_kbps;

  video_stat.avg_qp = qp.Mean();

  video_stat.avg_psnr = psnr.Mean();
  video_stat.min_psnr = psnr.Min();
  video_stat.avg_ssim = ssim.Mean();
  video_stat.min_ssim = ssim.Min();

  return video_stat;
}

FrameStatistic Stats::AggregateFrameStatistic(size_t frame_num,
                                              size_t spatial_layer_idx) {
  FrameStatistic frame_stat = *GetFrame(frame_num, spatial_layer_idx);

  while (spatial_layer_idx-- > 0) {
    // TODO(ssilkin): Can this be done to Videoprocessor?
    FrameStatistic* base_frame_stat = GetFrame(frame_num, spatial_layer_idx);
    frame_stat.encoded_frame_size_bytes +=
        base_frame_stat->encoded_frame_size_bytes;
    frame_stat.target_bitrate_kbps += base_frame_stat->target_bitrate_kbps;
  }

  return frame_stat;
}

size_t Stats::Size(size_t spatial_layer_idx) {
  return layer_idx_to_stats_[spatial_layer_idx].size();
}

void Stats::Clear() {
  layer_idx_to_stats_.clear();
}

}  // namespace test
}  // namespace webrtc
