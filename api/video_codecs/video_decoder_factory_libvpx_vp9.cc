/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_decoder_factory_libvpx_vp9.h"

#include "api/field_trials_view.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"

namespace webrtc {

std::vector<VideoDecoderFactoryItem> LibvpxVp9Decoders() {
  auto factory = [](const FieldTrialsView& experiments) {
    return VP9Decoder::Create();
  };
  std::vector<VideoDecoderFactoryItem> decoders;
  for (const SdpVideoFormat& format : SupportedVP9DecoderCodecs()) {
    decoders.emplace_back(format, factory);
  }
  return decoders;
}

}  // namespace webrtc
