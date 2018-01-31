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

#import "RTCI420Buffer+Private.h"

#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace webrtc {

CoreVideoFrameBuffer::CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer,
                                           int adapted_width,
                                           int adapted_height,
                                           int crop_width,
                                           int crop_height,
                                           int crop_x,
                                           int crop_y) {
  objc_pixel_buffer_ = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixel_buffer
                                                        adaptedWidth:adapted_width
                                                       adaptedHeight:adapted_height
                                                           cropWidth:crop_width
                                                          cropHeight:crop_height
                                                               cropX:crop_x
                                                               cropY:crop_y];
}

CoreVideoFrameBuffer::CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer) {
  objc_pixel_buffer_ = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixel_buffer];
}

CoreVideoFrameBuffer::~CoreVideoFrameBuffer() {
  objc_pixel_buffer_ = nil;
}

VideoFrameBuffer::Type CoreVideoFrameBuffer::type() const {
  return Type::kNative;
}

int CoreVideoFrameBuffer::width() const {
  return [objc_pixel_buffer_ width];
}

int CoreVideoFrameBuffer::height() const {
  return [objc_pixel_buffer_ height];
}

rtc::scoped_refptr<I420BufferInterface> CoreVideoFrameBuffer::ToI420() {
  RTCI420Buffer *i420Buffer = (RTCI420Buffer *)[objc_pixel_buffer_ toI420];
  return [i420Buffer nativeI420Buffer];
}

bool CoreVideoFrameBuffer::RequiresCropping() const {
  return [objc_pixel_buffer_ requiresCropping];
}

bool CoreVideoFrameBuffer::CropAndScaleTo(std::vector<uint8_t> *tmp_buffer,
                                          CVPixelBufferRef output_pixel_buffer) const {
  return [objc_pixel_buffer_ cropAndScaleTo:output_pixel_buffer withTempBuffer:tmp_buffer->data()];
}

}  // namespace webrtc
