/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_OPEN_H264_ADAPTER_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_OPEN_H264_ADAPTER_H_

#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"

namespace webrtc {
struct OpenH264EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats();
  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const Environment& env,
      const SdpVideoFormat& format);
  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format);
  static bool IsScalabilityModeSupported(ScalabilityMode scalability_mode);
};
}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_OPEN_H264_ADAPTER_H_
