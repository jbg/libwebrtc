/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/vpx_image.h"

#include <string.h>

#include "rtc_base/checks.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"

namespace webrtc {

namespace {

// Defines the dimension of a macro block. This is used to compute the active
// map for the encoder.
constexpr int kMacroBlockSize = 16;

}  // namespace

VpxImage::VpxImage(const DesktopSize& size, bool use_i444) {
  memset(&image_, 0, sizeof(vpx_image_t));

  // libvpx seems to require both to be assigned.
  image_.d_w = size.width();
  image_.w = size.width();
  image_.d_h = size.height();
  image_.h = size.height();

  // libvpx should derive chroma shifts from|fmt| but currently has a bug:
  // https://code.google.com/p/webm/issues/detail?id=627
  if (use_i444) {
    image_.fmt = VPX_IMG_FMT_I444;
    image_.x_chroma_shift = 0;
    image_.y_chroma_shift = 0;
  } else {  // I420
    image_.fmt = VPX_IMG_FMT_YV12;
    image_.x_chroma_shift = 1;
    image_.y_chroma_shift = 1;
  }

  // libyuv's fast-path requires 16-byte aligned pointers and strides, so pad
  // the Y, U and V planes' strides to multiples of 16 bytes.
  const int y_stride = ((image_.w - 1) & ~15) + 16;
  const int uv_unaligned_stride = y_stride >> image_.x_chroma_shift;
  const int uv_stride = ((uv_unaligned_stride - 1) & ~15) + 16;

  // libvpx accesses the source image in macro blocks, and will over-read
  // if the image is not padded out to the next macroblock: crbug.com/119633.
  // Pad the Y, U and V planes' height out to compensate.
  // Assuming macroblocks are 16x16, aligning the planes' strides above also
  // macroblock aligned them.
  const int y_rows =
      ((image_.h - 1) & ~(kMacroBlockSize - 1)) + kMacroBlockSize;
  const int uv_rows = y_rows >> image_.y_chroma_shift;

  // Allocate a YUV buffer large enough for the aligned data & padding.
  const int buffer_size = y_stride * y_rows + 2 * uv_stride * uv_rows;
  buffer_.reset(new uint8_t[buffer_size]);

  // Reset image value to 128 so we just need to fill in the y plane.
  memset(buffer_.get(), 128, buffer_size);

  // Fill in the information for |image_|.
  unsigned char* uchar_buffer =
      reinterpret_cast<unsigned char*>(buffer_.get());
  image_.planes[0] = uchar_buffer;
  image_.planes[1] = image_.planes[0] + y_stride * y_rows;
  image_.planes[2] = image_.planes[1] + uv_stride * uv_rows;
  image_.stride[0] = y_stride;
  image_.stride[1] = uv_stride;
  image_.stride[2] = uv_stride;
}

VpxImage::~VpxImage() = default;

const vpx_image_t& VpxImage::image() const {
  return image_;
}

DesktopSize VpxImage::size() const {
  return webrtc::DesktopSize(image().w, image().h);
}

void VpxImage::Draw(const DesktopFrame& frame) {
  RTC_DCHECK(frame.size().equals(size()));
  if (first_frame_) {
    DrawRect(frame, DesktopRect::MakeSize(frame.size()));
  } else {
    for (webrtc::DesktopRegion::Iterator r(frame.updated_region());
         !r.IsAtEnd();
         r.Advance()) {
      DrawRect(frame, r.rect());
    }
  }
}

void VpxImage::Export(DesktopFrame* frame) const {
  RTC_DCHECK_GE(frame->size().width(), image().w);
  RTC_DCHECK_GE(frame->size().height(), image().h);

  uint8_t* const rgb_data = frame->data();
  const int rgb_stride = frame->stride();
  const int y_stride = image().stride[0];
  RTC_DCHECK_EQ(image().stride[1], image().stride[2]);
  const int uv_stride = image().stride[1];
  const uint8_t* const y_data = image().planes[0];
  const uint8_t* const u_data = image().planes[1];
  const uint8_t* const v_data = image().planes[2];

  int (*ToARGB)(const uint8_t* src_y,
                int src_stride_y,
                const uint8_t* src_u,
                int src_stride_u,
                const uint8_t* src_v,
                int src_stride_v,
                uint8_t* dst_argb,
                int dst_stride_argb,
                int width,
                int height) = nullptr;

  switch (image().fmt) {
    case VPX_IMG_FMT_I444: {
      ToARGB = libyuv::I444ToARGB;
      break;
    }
    case VPX_IMG_FMT_YV12: {
      ToARGB = libyuv::I420ToARGB;
      break;
    }
    default: {
      RTC_NOTREACHED();
      break;
    }
  }

  RTC_DCHECK(ToARGB != nullptr);
  int result = ToARGB(y_data, y_stride,
                      u_data, uv_stride,
                      v_data, uv_stride,
                      rgb_data, rgb_stride,
                      image().w, image().h);
  RTC_DCHECK_EQ(result, 0);
}

void VpxImage::DrawRect(const DesktopFrame& frame, const DesktopRect& rect) {
  RTC_DCHECK(DesktopRect::MakeSize(frame.size()).ContainsRect(rect));

  // Convert the updated region to YUV ready for encoding.
  const uint8_t* const rgb_data = frame.data();
  const int rgb_stride = frame.stride();
  const int y_stride = image().stride[0];
  RTC_DCHECK_EQ(image().stride[1], image().stride[2]);
  const int uv_stride = image().stride[1];
  uint8_t* const y_data = image().planes[0];
  uint8_t* const u_data = image().planes[1];
  uint8_t* const v_data = image().planes[2];
  const int rgb_offset =
      rgb_stride * rect.top() + rect.left() * DesktopFrame::kBytesPerPixel;

  int result = -1;
  switch (image().fmt) {
    case VPX_IMG_FMT_I444: {
      int yuv_offset = uv_stride * rect.top() + rect.left();
      result = libyuv::ARGBToI444(rgb_data + rgb_offset, rgb_stride,
                                  y_data + yuv_offset, y_stride,
                                  u_data + yuv_offset, uv_stride,
                                  v_data + yuv_offset, uv_stride,
                                  rect.width(), rect.height());
      break;
    }
    case VPX_IMG_FMT_YV12: {
      int y_offset = y_stride * rect.top() + rect.left();
      int uv_offset = uv_stride * rect.top() / 2 + rect.left() / 2;
      result = libyuv::ARGBToI420(rgb_data + rgb_offset, rgb_stride,
                                  y_data + y_offset, y_stride,
                                  u_data + uv_offset, uv_stride,
                                  v_data + uv_offset, uv_stride,
                                  rect.width(), rect.height());
      break;
    }
    default: {
      RTC_NOTREACHED();
      break;
    }
  }
  RTC_DCHECK_EQ(result, 0);
}

}  // namespace webrtc
