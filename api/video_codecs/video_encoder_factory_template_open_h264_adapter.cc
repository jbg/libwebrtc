/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"

#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

namespace webrtc {

// TODO: bugs.webrtc.org/13573 - When OpenH264 is no longer a conditional build
//                               target remove #ifdefs.
#if defined(WEBRTC_USE_H264)

std::vector<SdpVideoFormat> OpenH264EncoderTemplateAdapter::SupportedFormats() {
  return SupportedH264Codecs(/*add_scalability_modes=*/true);
}

std::unique_ptr<VideoEncoder> OpenH264EncoderTemplateAdapter::CreateEncoder(
    const Environment& env,
    const SdpVideoFormat& format) {
  H264EncoderSettings settings;
  auto it = format.parameters.find(cricket::kH264FmtpPacketizationMode);
  if (it != format.parameters.end()) {
    if (it->second == "0") {
      // https://datatracker.ietf.org/doc/html/rfc6184#section-6.2
      settings.packetization_mode = H264PacketizationMode::SingleNalUnit;
    } else if (it->second == "1") {
      // https://datatracker.ietf.org/doc/html/rfc6184#section-6.3
      settings.packetization_mode = H264PacketizationMode::NonInterleaved;
    }
  }
  return CreateH264Encoder(env, settings);
}

std::unique_ptr<VideoEncoder> OpenH264EncoderTemplateAdapter::CreateEncoder(
    const SdpVideoFormat& format) {
  return H264Encoder::Create(cricket::CreateVideoCodec(format));
}

bool OpenH264EncoderTemplateAdapter::IsScalabilityModeSupported(
    ScalabilityMode scalability_mode) {
  return H264Encoder::SupportsScalabilityMode(scalability_mode);
}

#else  // !defined(WEBRTC_USE_H264)

std::vector<SdpVideoFormat> OpenH264EncoderTemplateAdapter::SupportedFormats() {
  return {};
}

std::unique_ptr<VideoEncoder> OpenH264EncoderTemplateAdapter::CreateEncoder(
    const Environment& env,
    const SdpVideoFormat& format) {
  return nullptr;
}

std::unique_ptr<VideoEncoder> OpenH264EncoderTemplateAdapter::CreateEncoder(
    const SdpVideoFormat& format) {
  return nullptr;
}

bool OpenH264EncoderTemplateAdapter::IsScalabilityModeSupported(
    ScalabilityMode scalability_mode) {
  return false;
}

#endif

}  // namespace webrtc
