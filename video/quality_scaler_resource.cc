/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/quality_scaler_resource.h"

#include <utility>

#include "api/rtp_parameters.h"
#include "api/video/video_stream_encoder_observer.h"

namespace webrtc {

namespace {

bool IsResolutionScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_FRAMERATE ||
         degradation_preference == DegradationPreference::BALANCED;
}

}  // namespace

QualityScalerResource::QualityScalerResource()
    : quality_scaler_(nullptr),
      quality_scaling_experiment_enabled_(QualityScalingExperiment::Enabled()) {
}

QualityScalerResource::~QualityScalerResource() {
  RTC_DCHECK(!is_started());
}

bool QualityScalerResource::is_started() const {
  return quality_scaler_.get();
}

void QualityScalerResource::StopCheckForOveruse() {
  quality_scaler_ = nullptr;
}

void QualityScalerResource::Configure(
    const VideoEncoder::EncoderInfo& encoder_info,
    DegradationPreference degradation_preference,
    VideoCodecType codec_type,
    int pixels) {
  const auto scaling_settings = encoder_info.scaling_settings;
  const bool quality_scaling_allowed =
      IsResolutionScalingEnabled(degradation_preference) &&
      scaling_settings.thresholds;

  if (quality_scaling_allowed) {
    if (!quality_scaler_) {
      // Use experimental thresholds if available.
      absl::optional<VideoEncoder::QpThresholds> experimental_thresholds;
      if (quality_scaling_experiment_enabled_) {
        experimental_thresholds =
            QualityScalingExperiment::GetQpThresholds(codec_type);
      }
      VideoEncoder::QpThresholds qp_thresholds =
          experimental_thresholds ? *experimental_thresholds
                                  : *(scaling_settings.thresholds);
      quality_scaler_ = std::make_unique<QualityScaler>(this, qp_thresholds);
    }
  } else {
    quality_scaler_.reset();
  }

  // Set the qp-thresholds to the balanced settings if balanced mode.
  if (degradation_preference == DegradationPreference::BALANCED &&
      quality_scaler_) {
    absl::optional<VideoEncoder::QpThresholds> thresholds =
        balanced_settings_.GetQpThresholds(codec_type, pixels);
    if (thresholds) {
      VideoEncoder::QpThresholds qp_thresholds = *thresholds;
      quality_scaler_->SetQpThresholds(qp_thresholds);
    }
  }
}

bool QualityScalerResource::QpFastFilterLow() {
  RTC_DCHECK(is_started());
  return quality_scaler_->QpFastFilterLow();
}

void QualityScalerResource::OnEncodeCompleted(const EncodedImage& encoded_image,
                                              int64_t time_sent_in_us) {
  if (quality_scaler_ && encoded_image.qp_ >= 0)
    quality_scaler_->ReportQp(encoded_image.qp_, time_sent_in_us);
}

void QualityScalerResource::OnFrameDropped(
    EncodedImageCallback::DropReason reason) {
  if (!quality_scaler_)
    return;
  switch (reason) {
    case EncodedImageCallback::DropReason::kDroppedByMediaOptimizations:
      quality_scaler_->ReportDroppedFrameByMediaOpt();
      break;
    case EncodedImageCallback::DropReason::kDroppedByEncoder:
      quality_scaler_->ReportDroppedFrameByEncoder();
      break;
  }
}

void QualityScalerResource::AdaptUp(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kQuality);
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

bool QualityScalerResource::AdaptDown(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kQuality);
  return OnResourceUsageStateMeasured(ResourceUsageState::kOveruse) !=
         ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
}

}  // namespace webrtc
