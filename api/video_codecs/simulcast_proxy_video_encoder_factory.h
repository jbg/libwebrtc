/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_SIMULCAST_PROXY_VIDEO_ENCODER_FACTORY_H_
#define API_VIDEO_CODECS_SIMULCAST_PROXY_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "media/engine/encoder_simulcast_proxy.h"

namespace webrtc {
// Unfortunately this word pasta factory needs to exist as a templetized
// alternative to BuiltinVideoEncoderFactory for two reasons:
//
//   1) The BuiltinVideoEncoderFactory wraps every encoder in a
//      EncoderSimulcastProxy.
//      // TODO(bugs.webrtc.org/13866): Remove EncoderSimulcastProxy wrapping.
//   2) The BuiltinVideoEncoderFactory accepts modified SdpVideoFormats, which
//      (incorrectly) allows applications to simply leak SDP level parameters
//      unrelated to the bitstream format into the VideoEncoderFactory.
//      // TODO(bugs.webrtc.org/13868): Remove MatchOriginalFormat.
//
// When both of these issues has been fixed this factory can be removed and
// VideoEncoderFactoryTemplate can be used directly.
//
// For documentation on how to add encoder implementations as template
// parameters please see the VideoEncoderFactoryTemplate.
template <typename... Ts>
class SimulcastProxyVideoEncoderFactory : public VideoEncoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return factory_.GetSupportedFormats();
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    if (auto original_format = MatchOriginalFormat(format); original_format) {
      return std::make_unique<EncoderSimulcastProxy>(&factory_,
                                                     *original_format);
    }

    return nullptr;
  }

  CodecSupport QueryCodecSupport(
      const SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const override {
    if (auto original_format = MatchOriginalFormat(format); original_format) {
      return factory_.QueryCodecSupport(*original_format, scalability_mode);
    }

    return {.is_supported = false};
  }

 private:
  absl::optional<SdpVideoFormat> MatchOriginalFormat(
      const SdpVideoFormat& format) const {
    const auto supported_formats = GetSupportedFormats();

    absl::optional<SdpVideoFormat> res;
    int best_parameter_match = 0;
    for (const auto& supported_format : supported_formats) {
      if (absl::EqualsIgnoreCase(supported_format.name, format.name)) {
        int matching_parameters = 0;
        for (const auto& kv : supported_format.parameters) {
          auto it = format.parameters.find(kv.first);
          if (it != format.parameters.end() && it->second == kv.second) {
            matching_parameters += 1;
          }
        }

        if (!res || matching_parameters > best_parameter_match) {
          res = supported_format;
          best_parameter_match = matching_parameters;
        }
      }
    }

    return res;
  }

  VideoEncoderFactoryTemplate<Ts...> factory_;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_SIMULCAST_PROXY_VIDEO_ENCODER_FACTORY_H_
