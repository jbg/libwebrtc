/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/video/planar_buffer_factory.h"

#include <algorithm>
#include <utility>

#include "api/video/i010_buffer.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

rtc::scoped_refptr<PlanarBuffer> PlanarBufferFactory::Create(
    VideoFrameBuffer::Type type,
    int width,
    int height) {
  switch (type) {
    case VideoFrameBuffer::Type::kI420:
      return I420Buffer::Create(width, height);
    case VideoFrameBuffer::Type::kI010:
      return I010Buffer::Create(width, height);
    default:
      RTC_NOTREACHED();
  }
  return nullptr;
}

rtc::scoped_refptr<PlanarBuffer> PlanarBufferFactory::Copy(
    const VideoFrameBuffer& src) {
  switch (src.type()) {
    case VideoFrameBuffer::Type::kI420:
      return I420Buffer::Copy(src);
    case VideoFrameBuffer::Type::kI010:
      return I010Buffer::Copy(*src.GetI010());
    default:
      RTC_NOTREACHED();
  }
  return nullptr;
}

rtc::scoped_refptr<PlanarBuffer> PlanarBufferFactory::Rotate(
    const VideoFrameBuffer& src,
    VideoRotation rotation) {
  switch (src.type()) {
    case VideoFrameBuffer::Type::kI420:
      return I420Buffer::Rotate(src, rotation);
    case VideoFrameBuffer::Type::kI010:
      return I010Buffer::Rotate(*src.GetI010(), rotation);
    default:
      RTC_NOTREACHED();
  }
  return nullptr;
}

rtc::scoped_refptr<PlanarBuffer> PlanarBufferFactory::CropAndScaleFrom(
    const VideoFrameBuffer& src,
    int offset_x,
    int offset_y,
    int crop_width,
    int crop_height) {
  switch (src.type()) {
    case VideoFrameBuffer::Type::kI420: {
      rtc::scoped_refptr<I420Buffer> buffer =
          I420Buffer::Create(crop_width, crop_height);
      buffer->CropAndScaleFrom(*src.GetI420(), offset_x, offset_y, crop_width,
                               crop_height);
      return buffer;
    }
    case VideoFrameBuffer::Type::kI010: {
      rtc::scoped_refptr<I010Buffer> buffer =
          I010Buffer::Create(crop_width, crop_height);
      buffer->CropAndScaleFrom(*src.GetI010(), offset_x, offset_y, crop_width,
                               crop_height);
      return buffer;
    }
    default:
      RTC_NOTREACHED();
  }
  return nullptr;
}

rtc::scoped_refptr<PlanarBuffer> PlanarBufferFactory::CropAndScaleFrom(
    const VideoFrameBuffer& src,
    int crop_width,
    int crop_height) {
  const int out_width =
      std::min(src.width(), crop_width * src.height() / crop_height);
  const int out_height =
      std::min(src.height(), crop_height * src.width() / crop_width);
  return CropAndScaleFrom(src, (src.width() - out_width) / 2,
                          (src.height() - out_height) / 2, out_width,
                          out_height);
}

rtc::scoped_refptr<PlanarBuffer> PlanarBufferFactory::ScaleFrom(
    const VideoFrameBuffer& src,
    int crop_width,
    int crop_height) {
  switch (src.type()) {
    case VideoFrameBuffer::Type::kI420: {
      rtc::scoped_refptr<I420Buffer> buffer =
          I420Buffer::Create(crop_width, crop_height);
      buffer->ScaleFrom(*src.GetI420());
      return buffer;
    }
    case VideoFrameBuffer::Type::kI010: {
      rtc::scoped_refptr<I010Buffer> buffer =
          I010Buffer::Create(crop_width, crop_height);
      buffer->ScaleFrom(*src.GetI010());
      return buffer;
    }
    default:
      RTC_NOTREACHED();
  }
  return nullptr;
}

}  // namespace webrtc
