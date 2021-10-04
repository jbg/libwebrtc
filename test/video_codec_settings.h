/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_VIDEO_CODEC_SETTINGS_H_
#define TEST_VIDEO_CODEC_SETTINGS_H_

#include "api/video_codecs/video_encoder.h"

namespace webrtc {
namespace test {

const uint16_t kTestWidth = 352;
const uint16_t kTestHeight = 288;
const uint32_t kTestFrameRate = 30;
const uint8_t kTestPayloadType = 100;
const int64_t kTestTimingFramesDelayMs = 200;
const uint16_t kTestOutlierFrameSizePercent = 250;

inline VideoEncodingConfig CodecSettings(absl::string_view codec_name) {
  VideoEncodingConfig config;
  config.set_render_resolution({kTestWidth, kTestHeight});
  config.set_start_bitrate(DataRate::BitsPerSec(300'000));
  config.set_min_bitrate(DataRate::BitsPerSec(30'000));

  config.set_max_framerate(Frequency::Hertz(kTestFrameRate));

  //  settings->active = true;

  //  settings->qpMax = 56;  // See webrtcvideoengine.h.

  //  settings->timing_frame_thresholds = {
  //      kTestTimingFramesDelayMs,
  //      kTestOutlierFrameSizePercent,
  //  };

  config.set_codec_name(codec_name);

  switch (config.codec_type()) {
    case kVideoCodecVP8:
      VideoEncoder::GetDefaultVp8Settings(config);
      //      *(settings->VP8()) = VideoEncoder::GetDefaultVp8Settings();
      break;
    case kVideoCodecVP9:
      VideoEncoder::GetDefaultVp9Settings(config);
      //      *(settings->VP9()) = VideoEncoder::GetDefaultVp9Settings();
      break;
    case kVideoCodecH264:
      VideoEncoder::GetDefaultH264Settings(config);
      // TODO(brandtr): Set `qpMax` here, when the OpenH264 wrapper supports it.
      //      *(settings->H264()) = VideoEncoder::GetDefaultH264Settings();
      break;
    default:
      break;
  }
  return config;
}
}  // namespace test
}  // namespace webrtc

#endif  // TEST_VIDEO_CODEC_SETTINGS_H_
