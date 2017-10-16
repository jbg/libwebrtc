/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_COREVIDEO_FRAME_BUFFER_H_
#define SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_COREVIDEO_FRAME_BUFFER_H_

#include <CoreVideo/CoreVideo.h>

#include <vector>

#include "common_video/include/video_frame_buffer.h"

namespace webrtc {

class CoreVideoFrameBuffer : public VideoFrameBuffer {
 public:
  explicit CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer);
  CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer,
                       int adapted_width,
                       int adapted_height,
                       int crop_width,
                       int crop_height,
                       int crop_x,
                       int crop_y);
  ~CoreVideoFrameBuffer() override;

  CVPixelBufferRef pixel_buffer();

  // Returns true if the internal pixel buffer needs to be cropped.
  bool RequiresCropping() const;
  // Crop and scales the internal pixel buffer to the output pixel buffer. The
  // tmp buffer is used for intermediary splitting the UV channels. This
  // function returns true if successful.
  bool CropAndScaleTo(std::vector<uint8_t>* tmp_buffer,
                      CVPixelBufferRef output_pixel_buffer) const;

 private:
  Type type() const override;
  int width() const override;
  int height() const override;
  rtc::scoped_refptr<I420BufferInterface> ToI420() override;

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_COREVIDEO_FRAME_BUFFER_H_
