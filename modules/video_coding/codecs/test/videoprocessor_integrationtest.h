/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "common_video/h264/h264_common.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/codecs/test/packet_manipulator.h"
#include "modules/video_coding/codecs/test/stats.h"
#include "modules/video_coding/codecs/test/test_config.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "test/gtest.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"
#include "test/testsupport/packet_reader.h"

namespace webrtc {
namespace test {

// Rates for the encoder and the frame number when to change profile.
struct RateProfile {
  int target_kbps;
  int input_fps;
  int frame_index_rate_update;
};

// Thresholds for the rate control metrics. The thresholds are defined for each
// rate update sequence. |max_num_frames_to_hit_target| is defined as number of
// frames, after a rate update is made to the encoder, for the encoder to reach
// |kMaxBitrateMismatchPercent| of new target rate.
struct RateControlThresholds {
  float max_avg_bitrate_mismatch_percent;
  float max_time_to_reach_target_bitrate_sec;
  float max_avg_framerate_mismatch_percent;
  float max_avg_buffer_level_sec;
  float max_max_key_frame_delay_sec;
  float max_max_delta_frame_delay_sec;
  size_t max_num_spatial_resizes;
  size_t max_num_key_frames;
};

// Thresholds for the quality metrics.
struct QualityThresholds {
  double min_avg_psnr;
  double min_min_psnr;
  double min_avg_ssim;
  double min_min_ssim;
};

struct BitstreamThresholds {
  size_t max_max_nalu_size_bytes;
};

// Should video files be saved persistently to disk for post-run visualization?
struct VisualizationParams {
  bool save_encoded_ivf;
  bool save_decoded_y4m;
};

// Integration test for video processor. Encodes+decodes a clip and
// writes it to the output directory. After completion, quality metrics
// (PSNR and SSIM) and rate control metrics are computed and compared to given
// thresholds, to verify that the quality and encoder response is acceptable.
// The rate control tests allow us to verify the behavior for changing bit rate,
// changing frame rate, frame dropping/spatial resize, and temporal layers.
// The thresholds for the rate control metrics are set to be fairly
// conservative, so failure should only happen when some significant regression
// or breakdown occurs.
class VideoProcessorIntegrationTest : public testing::Test {
 protected:
  // Verifies that all H.264 keyframes contain SPS/PPS/IDR NALUs.
  class H264KeyframeChecker : public TestConfig::EncodedFrameChecker {
   public:
    void CheckEncodedFrame(webrtc::VideoCodecType codec,
                           const EncodedImage& encoded_frame) const override;
  };

  VideoProcessorIntegrationTest();
  ~VideoProcessorIntegrationTest() override;

  void ProcessFramesAndMaybeVerify(
      const std::vector<RateProfile>& rate_profiles,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const std::vector<QualityThresholds>* quality_thresholds,
      const BitstreamThresholds* bs_thresholds,
      const VisualizationParams* visualization_params);

  // Config.
  TestConfig config_;

  // Can be used by all H.264 tests.
  const H264KeyframeChecker h264_keyframe_checker_;

 private:
  class CpuProcessTime;
  static const int kMaxNumTemporalLayers = 3;

  void CreateEncoderAndDecoder();
  void DestroyEncoderAndDecoder();
  void SetUpAndInitObjects(rtc::TaskQueue* task_queue,
                           const int initial_bitrate_kbps,
                           const int initial_framerate_fps,
                           const VisualizationParams* visualization_params);
  void ReleaseAndCloseObjects(rtc::TaskQueue* task_queue);

  void ProcessAllFrames(rtc::TaskQueue* task_queue,
                        const std::vector<RateProfile>& rate_profiles);
  void AnalyzeAllFrames(
      const std::vector<RateProfile>& rate_profiles,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const std::vector<QualityThresholds>* quality_thresholds,
      const BitstreamThresholds* bs_thresholds);

  std::vector<FrameStatistic> ExtractLayerStats(
      const int target_spatial_layer_number,
      const int target_temporal_layer_number,
      const int first_frame_number,
      const int last_frame_number,
      const bool combine_layers);

  void AnalyzeAndPrintStats(const std::vector<FrameStatistic>& stats,
                            const float target_bitrate_kbps,
                            const float target_framerate_fps,
                            const float input_duration_sec,
                            const RateControlThresholds* rc_thresholds,
                            const QualityThresholds* quality_thresholds,
                            const BitstreamThresholds* bs_thresholds);
  void PrintFrameByFrameStats(const std::vector<FrameStatistic>& stats);

  void PrintSettings() const;

  // Codecs.
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;

  // Helper objects.
  std::unique_ptr<FrameReader> analysis_frame_reader_;
  std::unique_ptr<FrameWriter> analysis_frame_writer_;
  std::unique_ptr<IvfFileWriter> encoded_frame_writer_;
  std::unique_ptr<FrameWriter> decoded_frame_writer_;
  PacketReader packet_reader_;
  std::unique_ptr<PacketManipulator> packet_manipulator_;
  Stats stats_;
  std::unique_ptr<VideoProcessor> processor_;
  std::unique_ptr<CpuProcessTime> cpu_process_time_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
