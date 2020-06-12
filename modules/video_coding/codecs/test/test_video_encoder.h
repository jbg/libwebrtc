/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_TEST_VIDEO_ENCODER_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_TEST_VIDEO_ENCODER_H_

#include <stdint.h>

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/encoded_image.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

// Wrap calls to `VideoEncoder::Encode`, collects and returns frame passed to
// the `EncodedImageCallback::OnEncodedImage`.
class TestVideoEncoder {
 public:
  struct Encoded {
    EncodedImage encoded_image;
    CodecSpecificInfo codec_specific_info;
  };

  // `encoder` should be initialized, but shouldn't have `EncoderCallback` set.
  explicit TestVideoEncoder(VideoEncoder& encoder) : encoder_(encoder) {}
  TestVideoEncoder(const TestVideoEncoder&) = delete;
  TestVideoEncoder& operator=(const TestVideoEncoder&) = delete;

  // Number of the input frames to pass to the encoder.
  TestVideoEncoder& SetNumInputFrames(int value);
  // Resolution of the input frames.
  TestVideoEncoder& SetResolution(RenderResolution value);

  std::vector<Encoded> Encode();

 private:
  VideoEncoder& encoder_;

  uint32_t rtp_timestamp_ = 1000;
  int num_input_frames_ = 1;
  int framerate_fps_ = 30;
  RenderResolution resolution_ = {320, 180};
};

inline TestVideoEncoder& TestVideoEncoder::SetNumInputFrames(int value) {
  RTC_DCHECK_GT(value, 0);
  num_input_frames_ = value;
  return *this;
}

inline TestVideoEncoder& TestVideoEncoder::SetResolution(
    RenderResolution value) {
  resolution_ = value;
  return *this;
}

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_CODECS_TEST_TEST_VIDEO_ENCODER_H_
