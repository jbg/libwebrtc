/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"

#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp9_profile.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"

namespace webrtc {

std::vector<SdpVideoFormat>
LibvpxVp9EncoderTemplateAdapter::SupportedFormats() {
  return SupportedVP9Codecs(/*add_scalability_modes=*/true);
}

std::unique_ptr<VideoEncoder> LibvpxVp9EncoderTemplateAdapter::CreateEncoder(
    const Environment& env,
    const SdpVideoFormat& format) {
  return CreateVp9Encoder(env,
                          {.profile = ParseSdpForVP9Profile(format.parameters)
                                          .value_or(VP9Profile::kProfile0)});
}

bool LibvpxVp9EncoderTemplateAdapter::IsScalabilityModeSupported(
    ScalabilityMode scalability_mode) {
  return VP9Encoder::SupportsScalabilityMode(scalability_mode);
}

}  // namespace webrtc
