/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/checks.h"

namespace webrtc {

VideoEncoderFactory::CodecSupport VideoEncoderFactory::QueryCodecSupport(
    const SdpVideoFormat& format,
    absl::optional<std::string> scalability_mode) const {
  // Default implementation, query for supported formats and check if the
  // specified format is supported. Returns false if scalability_mode is
  // specified.
  return {.is_supported = !scalability_mode.has_value() &&
                          format.IsCodecInList(GetSupportedFormats())};
}

std::unique_ptr<VideoEncoder> VideoEncoderFactory::Create(
    const Environment& env,
    const SdpVideoFormat& format) {
  return CreateVideoEncoder(format);
}

std::unique_ptr<VideoEncoder> VideoEncoderFactory::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  // Newer code shouldn't call this function,
  // Older code should implement it in derived classes.
  RTC_CHECK_NOTREACHED();
  return nullptr;
}

}  // namespace webrtc
