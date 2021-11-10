/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_DAV1D_DECODER_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_DAV1D_DECODER_H_

#include <memory>

#include "absl/base/attributes.h"
#include "api/video_codecs/video_decoder.h"
#include "common_video/include/video_frame_buffer_pool.h"

struct Dav1dContext;

namespace webrtc {

class Dav1dDecoder : public VideoDecoder {
 public:
  static std::unique_ptr<VideoDecoder> Create();

  Dav1dDecoder();
  Dav1dDecoder(const Dav1dDecoder&) = delete;
  Dav1dDecoder& operator=(const Dav1dDecoder&) = delete;

  ~Dav1dDecoder();

  bool Configure(const Settings& settings) override;
  int32_t Decode(const EncodedImage& encoded_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;
  int32_t Release() override;
  DecoderInfo GetDecoderInfo() const override;
  const char* ImplementationName() const override;

 private:
  VideoFrameBufferPool buffer_pool_;
  Dav1dContext* context_ = nullptr;
  DecodedImageCallback* decode_complete_callback_ = nullptr;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_DAV1D_DECODER_H_
