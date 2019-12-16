/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENCODED_FRAME_TRANSFORM_INTERFACE_H_
#define API_ENCODED_FRAME_TRANSFORM_INTERFACE_H_

#include "api/array_view.h"
#include "api/video/encoded_frame.h"
#include "api/video/encoded_image.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// Sender
class EncodedTransformableFrame {
 public:
  EncodedTransformableFrame(
      std::unique_ptr<EncodedImage> encoded_image,
      std::unique_ptr<CodecSpecificInfo> codec_specific_info,
      std::unique_ptr<RTPFragmentationHeader> fragmentation)
      : encoded_image_(std::move(encoded_image)),
        codec_specific_info_(std::move(codec_specific_info)),
        fragmentation_(std::move(fragmentation)) {}

  uint8_t* data() { return encoded_image_->data(); }
  const EncodedImage& encoded_image() { return *encoded_image_.get(); }
  CodecSpecificInfo* codec_specific_info() {
    return codec_specific_info_.get();
  }
  RTPFragmentationHeader* fragmentation() { return fragmentation_.get(); }

 private:
  std::unique_ptr<EncodedImage> encoded_image_ = nullptr;
  std::unique_ptr<CodecSpecificInfo> codec_specific_info_ = nullptr;
  std::unique_ptr<RTPFragmentationHeader> fragmentation_ = nullptr;
};

class TransformedFrameCallback {
 public:
  virtual void OnTransformedFrame(
      std::unique_ptr<EncodedTransformableFrame> frame) = 0;

 protected:
  virtual ~TransformedFrameCallback() = default;
};

class EncodedFrameTransformInterface {
 public:
  virtual ~EncodedFrameTransformInterface() = default;

  virtual void RegisterTransformedFrameCallback(TransformedFrameCallback*) = 0;
  virtual void TransformFrame(
      std::unique_ptr<EncodedTransformableFrame> frame) = 0;
};

// Receiver
class TransformedReceivedFrameCallback {
 public:
  virtual void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) = 0;

 protected:
  virtual ~TransformedReceivedFrameCallback() = default;
};

class ReceivedFrameTransformInterface {
 public:
  virtual ~ReceivedFrameTransformInterface() = default;

  virtual void RegisterTransformedFrameCallback(
      TransformedReceivedFrameCallback*) = 0;
  virtual void TransformFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) = 0;
};
}  // namespace webrtc

#endif  // API_ENCODED_FRAME_TRANSFORM_INTERFACE_H_
