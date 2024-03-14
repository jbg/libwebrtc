/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"

#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/environment/environment.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp8/vp8_scalability.h"

namespace webrtc {

std::vector<SdpVideoFormat>
LibvpxVp8EncoderTemplateAdapter::SupportedFormats() {
  absl::InlinedVector<ScalabilityMode, kScalabilityModeCount> scalability_modes;
  for (const auto scalability_mode : kVP8SupportedScalabilityModes) {
    scalability_modes.push_back(scalability_mode);
  }

  return {SdpVideoFormat("VP8", CodecParameterMap(), scalability_modes)};
}

std::unique_ptr<VideoEncoder> LibvpxVp8EncoderTemplateAdapter::CreateEncoder(
    const Environment& env,
    const SdpVideoFormat& format) {
  return CreateVp8Encoder(env);
}

std::unique_ptr<VideoEncoder> LibvpxVp8EncoderTemplateAdapter::CreateEncoder(
    const SdpVideoFormat& format) {
  return VP8Encoder::Create();
}

bool LibvpxVp8EncoderTemplateAdapter::IsScalabilityModeSupported(
    ScalabilityMode scalability_mode) {
  return VP8SupportsScalabilityMode(scalability_mode);
}

}  // namespace webrtc
