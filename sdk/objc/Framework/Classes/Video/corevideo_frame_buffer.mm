/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h"

#import "WebRTC/RTCVideoFrameBuffer.h"

#import "RTCI420Buffer+Private.h"

namespace webrtc {

class CoreVideoFrameBuffer::Impl {
 public:
  RTCCVPixelBuffer *rtc_cv_pixel_buffer_;
  Impl(CVPixelBufferRef pixel_buffer,
       int adapted_width,
       int adapted_height,
       int crop_width,
       int crop_height,
       int crop_x,
       int crop_y) {
    rtc_cv_pixel_buffer_ = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixel_buffer
                                                            adaptedWidth:adapted_width
                                                           adaptedHeight:adapted_height
                                                               cropWidth:crop_width
                                                              cropHeight:crop_height
                                                                   cropX:crop_x
                                                                   cropY:crop_y];
  }
  explicit Impl(CVPixelBufferRef pixel_buffer) {
    rtc_cv_pixel_buffer_ = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixel_buffer];
  }
};

CoreVideoFrameBuffer::CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer,
                                           int adapted_width,
                                           int adapted_height,
                                           int crop_width,
                                           int crop_height,
                                           int crop_x,
                                           int crop_y)
    : impl_(std::unique_ptr<CoreVideoFrameBuffer::Impl>(new CoreVideoFrameBuffer::Impl(
          pixel_buffer, adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y))) {}

CoreVideoFrameBuffer::CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer)
    : impl_(std::unique_ptr<CoreVideoFrameBuffer::Impl>(
          new CoreVideoFrameBuffer::Impl(pixel_buffer))) {}

CoreVideoFrameBuffer::~CoreVideoFrameBuffer() {}

CVPixelBufferRef CoreVideoFrameBuffer::pixel_buffer() {
  return impl_->rtc_cv_pixel_buffer_.pixelBuffer;
}

VideoFrameBuffer::Type CoreVideoFrameBuffer::type() const {
  return Type::kNative;
}

int CoreVideoFrameBuffer::width() const {
  return [impl_->rtc_cv_pixel_buffer_ width];
}

int CoreVideoFrameBuffer::height() const {
  return [impl_->rtc_cv_pixel_buffer_ height];
}

rtc::scoped_refptr<I420BufferInterface> CoreVideoFrameBuffer::ToI420() {
  RTCI420Buffer *i420buffer = [impl_->rtc_cv_pixel_buffer_ toI420];
  return [i420buffer nativeI420Buffer];
}

bool CoreVideoFrameBuffer::RequiresCropping() const {
  return [impl_->rtc_cv_pixel_buffer_ requiresCropping];
}

bool CoreVideoFrameBuffer::CropAndScaleTo(std::vector<uint8_t> *tmp_buffer,
                                          CVPixelBufferRef output_pixel_buffer) const {
  if ([impl_->rtc_cv_pixel_buffer_ requiresCropping]) {
    int dstWidth = CVPixelBufferGetWidth(output_pixel_buffer);
    int dstHeight = CVPixelBufferGetHeight(output_pixel_buffer);
    int size = [impl_->rtc_cv_pixel_buffer_ bufferSizeForCroppingAndScalingToWidth:dstWidth
                                                                            height:dstHeight];
    tmp_buffer->resize(size);
  } else {
    tmp_buffer->clear();
  }
  tmp_buffer->shrink_to_fit();

  return [impl_->rtc_cv_pixel_buffer_ cropAndScaleTo:output_pixel_buffer
                                      withTempBuffer:tmp_buffer->data()];
}

}  // namespace webrtc
