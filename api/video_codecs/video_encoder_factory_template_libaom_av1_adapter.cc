/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"

#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/video_codecs/sdp_video_format.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

namespace webrtc {

std::vector<SdpVideoFormat>
LibaomAv1EncoderTemplateAdapter::SupportedFormats() {
  absl::InlinedVector<ScalabilityMode, kScalabilityModeCount>
      scalability_modes = LibaomAv1EncoderSupportedScalabilityModes();
  return {SdpVideoFormat("AV1", CodecParameterMap(), scalability_modes)};
}

std::unique_ptr<VideoEncoder> LibaomAv1EncoderTemplateAdapter::CreateEncoder(
    const Environment& env,
    const SdpVideoFormat& format) {
  return CreateLibaomAv1Encoder(env);
}

std::unique_ptr<VideoEncoder> LibaomAv1EncoderTemplateAdapter::CreateEncoder(
    const SdpVideoFormat& format) {
  return CreateLibaomAv1Encoder();
}

bool LibaomAv1EncoderTemplateAdapter::IsScalabilityModeSupported(
    ScalabilityMode scalability_mode) {
  return LibaomAv1EncoderSupportsScalabilityMode(scalability_mode);
}

}  // namespace webrtc
