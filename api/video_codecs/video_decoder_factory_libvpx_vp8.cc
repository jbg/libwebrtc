/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_decoder_factory_libvpx_vp8.h"

#include "api/field_trials_view.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"

namespace webrtc {

std::vector<VideoDecoderFactoryItem> LibvpxVp8Decoders() {
  std::vector<VideoDecoderFactoryItem> decoders;
  decoders.emplace_back(SdpVideoFormat("VP8"), CreateVp8Decoder);
  return decoders;
}

}  // namespace webrtc
