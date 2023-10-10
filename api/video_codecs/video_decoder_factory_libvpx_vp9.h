/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_LIBVPX_VP9_H_
#define API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_LIBVPX_VP9_H_

#include <vector>

#include "api/video_codecs/video_decoder_factory_item.h"

namespace webrtc {

std::vector<VideoDecoderFactoryItem> LibvpxVp9Decoders();

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_LIBVPX_VP9_H_
