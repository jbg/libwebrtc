/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_

#include "api/fec_controller.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/deprecation.h"

namespace webrtc {

class Clock;
class EncodedImageCallback;
class VideoDecoder;
class VideoEncoder;
struct CodecSpecificInfo;

// Used to indicate which decode with errors mode should be used.
enum VCMDecodeErrorMode {
  kNoErrors,         // Never decode with errors. Video will freeze
                     // if nack is disabled.
  kSelectiveErrors,  // Frames that are determined decodable in
                     // VCMSessionInfo may be decoded with missing
                     // packets. As not all incomplete frames will be
                     // decodable, video will freeze if nack is disabled.
  kWithErrors        // Release frames as needed. Errors may be
                     // introduced as some encoded frames may not be
                     // complete.
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_
