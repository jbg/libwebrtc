/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_decoder.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_encoder.h"
#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"
#include "test/video_renderer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

namespace {

class VideoWriter final : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  VideoWriter(test::VideoFrameWriter* video_writer)
      : video_writer_(video_writer) {}
  ~VideoWriter() override = default;

  void OnFrame(const VideoFrame& frame) override {
    bool result = video_writer_->WriteFrame(frame);
    RTC_CHECK(result) << "Failed to write frame";
  }

 private:
  test::VideoFrameWriter* video_writer_;
};

class AnalyzingFramePreprocessor
    : public test::TestVideoCapturer::FramePreprocessor {
 public:
  AnalyzingFramePreprocessor(
      std::string stream_label,
      VideoQualityAnalyzerInterface* analyzer,
      std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks)
      : stream_label_(std::move(stream_label)),
        analyzer_(analyzer),
        sinks_(std::move(sinks)) {}
  ~AnalyzingFramePreprocessor() override = default;

  VideoFrame Preprocess(const VideoFrame& source_frame) override {
    // Copy VideoFrame to be able to set id on it.
    VideoFrame frame = source_frame;
    uint16_t frame_id = analyzer_->OnFrameCaptured(stream_label_, frame);
    frame.set_id(frame_id);

    for (auto& sink : sinks_) {
      sink->OnFrame(frame);
    }
    return frame;
  }

 private:
  const std::string stream_label_;
  VideoQualityAnalyzerInterface* const analyzer_;
  const std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      sinks_;
};

}  // namespace

VideoQualityAnalyzerInjectionHelper::VideoQualityAnalyzerInjectionHelper(
    std::unique_ptr<VideoQualityAnalyzerInterface> analyzer,
    EncodedImageDataInjector* injector,
    EncodedImageDataExtractor* extractor,
    MediaDumpManager* media_dump_manager)
    : analyzer_(std::move(analyzer)),
      injector_(injector),
      extractor_(extractor),
      media_dump_manager_(media_dump_manager),
      encoding_entities_id_generator_(std::make_unique<IntIdGenerator>(1)) {
  RTC_DCHECK(injector_);
  RTC_DCHECK(extractor_);
}
VideoQualityAnalyzerInjectionHelper::~VideoQualityAnalyzerInjectionHelper() =
    default;

std::unique_ptr<VideoEncoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> delegate,
    double bitrate_multiplier,
    std::map<std::string, absl::optional<int>> stream_required_spatial_index)
    const {
  return std::make_unique<QualityAnalyzingVideoEncoderFactory>(
      std::move(delegate), bitrate_multiplier,
      std::move(stream_required_spatial_index),
      encoding_entities_id_generator_.get(), injector_, analyzer_.get());
}

std::unique_ptr<VideoDecoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> delegate) const {
  return std::make_unique<QualityAnalyzingVideoDecoderFactory>(
      std::move(delegate), encoding_entities_id_generator_.get(), extractor_,
      analyzer_.get());
}

std::unique_ptr<test::TestVideoCapturer::FramePreprocessor>
VideoQualityAnalyzerInjectionHelper::CreateFramePreprocessor(
    const VideoConfig& config,
    test::VideoFrameWriter* writer) {
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  if (writer) {
    sinks.push_back(std::make_unique<VideoWriter>(writer));
  }
  if (config.show_on_screen) {
    sinks.push_back(absl::WrapUnique(
        test::VideoRenderer::Create((*config.stream_label + "-capture").c_str(),
                                    config.width, config.height)));
  }
  {
    rtc::CritScope crit(&lock_);
    known_video_configs_.insert({*config.stream_label, config});
  }
  return std::make_unique<AnalyzingFramePreprocessor>(
      std::move(*config.stream_label), analyzer_.get(), std::move(sinks));
}

std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>
VideoQualityAnalyzerInjectionHelper::CreateVideoSink(
    const VideoConfig& config,
    test::VideoFrameWriter* writer) {
  return std::make_unique<AnalyzingVideoSink>(this);
}

std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>
VideoQualityAnalyzerInjectionHelper::CreateVideoSink() {
  return std::make_unique<AnalyzingVideoSink>(this);
}

void VideoQualityAnalyzerInjectionHelper::Start(std::string test_case_name,
                                                int max_threads_count) {
  analyzer_->Start(std::move(test_case_name), max_threads_count);
}

void VideoQualityAnalyzerInjectionHelper::OnStatsReports(
    const std::string& pc_label,
    const StatsReports& stats_reports) {
  analyzer_->OnStatsReports(pc_label, stats_reports);
}

void VideoQualityAnalyzerInjectionHelper::Stop() {
  analyzer_->Stop();
}

void VideoQualityAnalyzerInjectionHelper::OnFrame(const VideoFrame& frame) {
  if (IsDummyFrameBuffer(frame.video_frame_buffer()->ToI420())) {
    // This is dummy frame, so we  don't need to process it further.
    return;
  }
  analyzer_->OnFrameRendered(frame);
  std::string stream_label = analyzer_->GetStreamLabel(frame.id());
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>* sinks =
      GetSinks(stream_label);
  if (sinks == nullptr) {
    return;
  }
  for (auto& sink : *sinks) {
    sink->OnFrame(frame);
  }
}

std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>*
VideoQualityAnalyzerInjectionHelper::GetSinks(const std::string& stream_label) {
  rtc::CritScope crit(&lock_);
  auto sinks_it = sinks_.find(stream_label);
  if (sinks_it != sinks_.end()) {
    return &sinks_it->second;
  }
  auto it = known_video_configs_.find(stream_label);
  RTC_DCHECK(it != known_video_configs_.end())
      << "No video config for stream " << stream_label;
  const VideoConfig& config = it->second;

  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  test::VideoFrameWriter* writer = media_dump_manager_->MaybeCreateVideoWriter(
      config.output_dump_file_name, config);
  if (writer) {
    sinks.push_back(std::make_unique<VideoWriter>(writer));
  }
  if (config.show_on_screen) {
    sinks.push_back(absl::WrapUnique(
        test::VideoRenderer::Create((*config.stream_label + "-render").c_str(),
                                    config.width, config.height)));
  }
  sinks_.insert({stream_label, std::move(sinks)});
  return &(sinks_.find(stream_label)->second);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
