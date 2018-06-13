/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_PLANAR_BUFFER_FACTORY_H_
#define API_VIDEO_PLANAR_BUFFER_FACTORY_H_

#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"

namespace webrtc {

// Class for holding static utility functions for I420Buffer and I010Buffer.
class PlanarBufferFactory {
 public:
  // Create a new buffer.
  static rtc::scoped_refptr<PlanarBuffer> Create(VideoFrameBuffer::Type type,
                                                 int width,
                                                 int height);

  // Create a new buffer and copy the pixel data.
  static rtc::scoped_refptr<PlanarBuffer> Copy(const VideoFrameBuffer& src);

  // Returns a rotated copy of |src|.
  static rtc::scoped_refptr<PlanarBuffer> Rotate(const VideoFrameBuffer& src,
                                                 VideoRotation rotation);

  // Create a new buffer, scale the cropped area of |src| to the size of
  // the new buffer, and write the result.
  static rtc::scoped_refptr<PlanarBuffer> CropAndScaleFrom(
      const VideoFrameBuffer& src,
      int offset_x,
      int offset_y,
      int crop_width,
      int crop_height);
  static rtc::scoped_refptr<PlanarBuffer> CropAndScaleFrom(
      const VideoFrameBuffer& src,
      int crop_width,
      int crop_height);

  // Create a new buffer, scale all of |src| to the size of the new buffer
  // buffer, with no cropping.
  static rtc::scoped_refptr<PlanarBuffer> ScaleFrom(const VideoFrameBuffer& src,
                                                    int crop_width,
                                                    int crop_height);
};

}  // namespace webrtc

#endif  // API_VIDEO_PLANAR_BUFFER_FACTORY_H_
