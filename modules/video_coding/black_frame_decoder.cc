/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/black_frame_decoder.h"

#include <cstdint>

#include "api/scoped_refptr.h"
#include "modules/video_coding/include/video_error_codes.h"
namespace webrtc {

BlackFrameDecoder::BlackFrameDecoder(SdpVideoFormat format) : format_(format) {}

bool BlackFrameDecoder::Configure(const Settings& settings) {
  return true;
}

int32_t BlackFrameDecoder::Decode(const webrtc::EncodedImage& input_image,
                                  bool missing_frames,
                                  int64_t render_time_ms) {
  // For keyframes store the size and reuse in delta frames.
  // TODO(bugs.webrtc.org/14251): AV1 does not set these values.
  if (input_image._frameType == VideoFrameType::kVideoFrameKey) {
    width_ = input_image._encodedWidth;
    height_ = input_image._encodedHeight;
  }
  webrtc::VideoFrame video_frame =
      CreateFrame(width_, height_, input_image.Timestamp());
  decode_complete_callback_->Decoded(video_frame, absl::nullopt, absl::nullopt);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t BlackFrameDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t BlackFrameDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* BlackFrameDecoder::ImplementationName() const {
  return "BlackFrameDecoder";
}

webrtc::VideoFrame BlackFrameDecoder::CreateFrame(int width,
                                                  int height,
                                                  uint32_t timestamp) {
  RTC_DCHECK(width);
  RTC_DCHECK(height);
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(width, height);
  webrtc::I420Buffer::SetBlack(buffer.get());
  return webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_timestamp_rtp(timestamp)
      .build();
}

}  // namespace webrtc
