/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_source_controller.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "absl/types/optional.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

namespace {

int OptionalSizeTToInt(const absl::optional<size_t>& optional) {
  return optional.has_value() ? static_cast<int>(optional.value())
                              : std::numeric_limits<int>::max();
}

absl::optional<int> OptionalSizeTToOptionalInt(
    const absl::optional<size_t>& optional) {
  return optional.has_value()
             ? absl::optional<int>(
                   rtc::dchecked_cast<int, size_t>(optional.value()))
             : absl::nullopt;
}

int OptionalDoubleToInt(const absl::optional<double>& optional) {
  return optional.has_value() ? static_cast<int>(optional.value())
                              : std::numeric_limits<int>::max();
}

}  // namespace

VideoSourceController::VideoSourceController(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    rtc::VideoSourceInterface<VideoFrame>* source)
    : sink_(sink),
      source_(source),
      degradation_preference_(DegradationPreference::DISABLED) {}

void VideoSourceController::SetSource(
    rtc::VideoSourceInterface<VideoFrame>* source,
    DegradationPreference degradation_preference) {
  rtc::VideoSourceInterface<VideoFrame>* old_source;
  rtc::VideoSinkWants wants;
  {
    rtc::CritScope lock(&crit_);
    old_source = source_;
    source_ = source;
    degradation_preference_ = degradation_preference;
    wants = CurrentSettingsToSinkWantsInternal();
  }
  if (old_source != source && old_source)
    old_source->RemoveSink(sink_);
  if (!source)
    return;
  source->AddOrUpdateSink(sink_, wants);
}

void VideoSourceController::PushSourceSinkSettings() {
  rtc::VideoSourceInterface<VideoFrame>* source;
  rtc::VideoSinkWants wants;
  {
    rtc::CritScope lock(&crit_);
    source = source_;
    wants = CurrentSettingsToSinkWantsInternal();
  }
  if (!source)
    return;
  source->AddOrUpdateSink(sink_, wants);
}

VideoSourceRestrictions VideoSourceController::restrictions() const {
  rtc::CritScope lock(&crit_);
  return restrictions_;
}

absl::optional<size_t> VideoSourceController::pixels_per_frame_upper_limit()
    const {
  rtc::CritScope lock(&crit_);
  return pixels_per_frame_upper_limit_;
}

absl::optional<double> VideoSourceController::frame_rate_upper_limit() const {
  rtc::CritScope lock(&crit_);
  return frame_rate_upper_limit_;
}

bool VideoSourceController::rotation_applied() const {
  rtc::CritScope lock(&crit_);
  return rotation_applied_;
}

int VideoSourceController::resolution_alignment() const {
  rtc::CritScope lock(&crit_);
  return resolution_alignment_;
}

void VideoSourceController::SetRestrictions(
    VideoSourceRestrictions restrictions) {
  rtc::CritScope lock(&crit_);
  restrictions_ = std::move(restrictions);
}

void VideoSourceController::SetPixelsPerFrameUpperLimit(
    absl::optional<size_t> pixels_per_frame_upper_limit) {
  rtc::CritScope lock(&crit_);
  pixels_per_frame_upper_limit_ = std::move(pixels_per_frame_upper_limit);
}

void VideoSourceController::SetFrameRateUpperLimit(
    absl::optional<double> frame_rate_upper_limit) {
  rtc::CritScope lock(&crit_);
  frame_rate_upper_limit_ = std::move(frame_rate_upper_limit);
}

void VideoSourceController::SetRotationApplied(bool rotation_applied) {
  rtc::CritScope lock(&crit_);
  rotation_applied_ = rotation_applied;
}

void VideoSourceController::SetResolutionAlignment(int resolution_alignment) {
  rtc::CritScope lock(&crit_);
  resolution_alignment_ = resolution_alignment;
}

rtc::VideoSinkWants VideoSourceController::CurrentSettingsToSinkWants() const {
  rtc::CritScope lock(&crit_);
  return CurrentSettingsToSinkWantsInternal();
}

rtc::VideoSinkWants VideoSourceController::CurrentSettingsToSinkWantsInternal()
    const {
  rtc::VideoSinkWants wants;
  wants.rotation_applied = rotation_applied_;
  // |wants.black_frames| is not used, it always has its default value false.
  wants.max_pixel_count =
      OptionalSizeTToInt(restrictions_.max_pixels_per_frame());
  wants.target_pixel_count =
      OptionalSizeTToOptionalInt(restrictions_.target_pixels_per_frame());
  wants.max_framerate_fps = OptionalDoubleToInt(restrictions_.max_frame_rate());
  wants.resolution_alignment = resolution_alignment_;
  {
    // Clear any constraints from the current sink wants that don't apply to
    // the used degradation_preference.
    switch (degradation_preference_) {
      case DegradationPreference::BALANCED:
        break;
      case DegradationPreference::MAINTAIN_FRAMERATE:
        wants.max_framerate_fps = std::numeric_limits<int>::max();
        break;
      case DegradationPreference::MAINTAIN_RESOLUTION:
        wants.max_pixel_count = std::numeric_limits<int>::max();
        wants.target_pixel_count.reset();
        break;
      case DegradationPreference::DISABLED:
        wants.max_pixel_count = std::numeric_limits<int>::max();
        wants.target_pixel_count.reset();
        wants.max_framerate_fps = std::numeric_limits<int>::max();
    }
  }
  if (pixels_per_frame_upper_limit_.has_value()) {
    wants.max_pixel_count =
        std::min(wants.max_pixel_count,
                 OptionalSizeTToInt(pixels_per_frame_upper_limit_));
  }
  if (frame_rate_upper_limit_.has_value()) {
    wants.max_framerate_fps = std::min(
        wants.max_framerate_fps, OptionalDoubleToInt(frame_rate_upper_limit_));
  }
  return wants;
}

}  // namespace webrtc
