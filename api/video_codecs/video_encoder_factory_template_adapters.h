/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_ADAPTERS_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_ADAPTERS_H_

#include <memory>
#include <vector>

#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/svc/create_scalability_structure.h"

namespace webrtc {
struct LibvpxVp8EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return {SdpVideoFormat("VP8")};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return VP8Encoder::Create();
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return VP8Encoder::SupportsScalabilityMode(scalability_mode);
  }
};

struct LibvpxVp9EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return SupportedVP9Codecs();
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return VP9Encoder::Create();
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return VP9Encoder::SupportsScalabilityMode(scalability_mode);
  }
};

struct OpenH264EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return SupportedH264Codecs();
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return H264Encoder::Create(cricket::VideoCodec(format));
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return H264Encoder::SupportsScalabilityMode(scalability_mode);
  }
};

struct LibaomAv1EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return {SdpVideoFormat("AV1")};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return CreateLibaomAv1Encoder();
  }

  static bool IsScalabilityModeSupported(absl::string_view scalability_mode) {
    // For libaom AV1, the scalability mode is supported if we can create the
    // scalability structure.
    return ScalabilityStructureConfig(scalability_mode) != absl::nullopt;
  }
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_ADAPTERS_H_
