/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/video/i420_buffer.h"

#include <string.h>

#include <algorithm>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/keep_ref_until_done.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/scale.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace webrtc {

namespace {

int BytesPerPixel(PlanarYuvBuffer::BitDepth bit_depth) {
  int bytes_per_pixel = 1;
  switch (bit_depth) {
    case PlanarYuvBuffer::BitDepth::kBitDepth8:
      bytes_per_pixel = 1;
      break;
    case PlanarYuvBuffer::BitDepth::kBitDepth10:
      bytes_per_pixel = 2;
      break;
  }
  return bytes_per_pixel;
}

int I420DataSize(int height,
                 int stride_y,
                 int stride_u,
                 int stride_v,
                 PlanarYuvBuffer::BitDepth bit_depth) {
  return BytesPerPixel(bit_depth) *
         (stride_y * height + (stride_u + stride_v) * ((height + 1) / 2));
}

}  // namespace

I420Buffer::I420Buffer(int width,
                       int height,
                       PlanarYuvBuffer::BitDepth bit_depth)
    : I420Buffer(width,
                 height,
                 width * BytesPerPixel(bit_depth),
                 ((width + 1) / 2) * BytesPerPixel(bit_depth),
                 ((width + 1) / 2) * BytesPerPixel(bit_depth),
                 bit_depth) {}

I420Buffer::I420Buffer(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v,
                       PlanarYuvBuffer::BitDepth bit_depth)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_u_(stride_u),
      stride_v_(stride_v),
      bit_depth_(bit_depth),
      data_(static_cast<uint8_t*>(AlignedMalloc(
          I420DataSize(height, stride_y, stride_u, stride_v, bit_depth),
          kBufferAlignment))) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GE(stride_y, width);
  RTC_DCHECK_GE(stride_u, (width + 1) / 2);
  RTC_DCHECK_GE(stride_v, (width + 1) / 2);
}

I420Buffer::~I420Buffer() {
}

// static
rtc::scoped_refptr<I420Buffer>
I420Buffer::Create(int width, int height, PlanarYuvBuffer::BitDepth bit_depth) {
  return new rtc::RefCountedObject<I420Buffer>(width, height, bit_depth);
}

// static
rtc::scoped_refptr<I420Buffer> I420Buffer::Create(
    int width,
    int height,
    int stride_y,
    int stride_u,
    int stride_v,
    PlanarYuvBuffer::BitDepth bit_depth) {
  return new rtc::RefCountedObject<I420Buffer>(width, height, stride_y,
                                               stride_u, stride_v, bit_depth);
}
// static
rtc::scoped_refptr<I420Buffer> I420Buffer::Copy(
    const I420BufferInterface& source) {
  return Copy(source.width(), source.height(), source.DataY(), source.StrideY(),
              source.DataU(), source.StrideU(), source.DataV(),
              source.StrideV(), source.bit_depth());
}

// static
rtc::scoped_refptr<I420Buffer> I420Buffer::Copy(
    int width,
    int height,
    const uint8_t* data_y,
    int stride_y,
    const uint8_t* data_u,
    int stride_u,
    const uint8_t* data_v,
    int stride_v,
    PlanarYuvBuffer::BitDepth bit_depth) {
  // Note: May use different strides than the input data.
  rtc::scoped_refptr<I420Buffer> buffer = Create(width, height, bit_depth);
  switch (bit_depth) {
    case PlanarYuvBuffer::BitDepth::kBitDepth8:
      RTC_CHECK_EQ(0,
                   libyuv::I420Copy(data_y, stride_y, data_u, stride_u, data_v,
                                    stride_v, buffer->MutableDataY(),
                                    buffer->StrideY(), buffer->MutableDataU(),
                                    buffer->StrideU(), buffer->MutableDataV(),
                                    buffer->StrideV(), width, height));
      break;
    case PlanarYuvBuffer::BitDepth::kBitDepth10:
      RTC_CHECK_EQ(0,
                   libyuv::I010Copy(
                       reinterpret_cast<const uint16_t*>(data_y), stride_y / 2,
                       reinterpret_cast<const uint16_t*>(data_u), stride_u / 2,
                       reinterpret_cast<const uint16_t*>(data_v), stride_v / 2,
                       reinterpret_cast<uint16_t*>(buffer->MutableDataY()),
                       buffer->StrideY() / 2,
                       reinterpret_cast<uint16_t*>(buffer->MutableDataU()),
                       buffer->StrideU() / 2,
                       reinterpret_cast<uint16_t*>(buffer->MutableDataV()),
                       buffer->StrideV() / 2, width, height));
      break;
  }
  return buffer;
}

// static
rtc::scoped_refptr<I420Buffer> I420Buffer::Rotate(
    const I420BufferInterface& src,
    VideoRotation rotation) {
  if (rotation == webrtc::kVideoRotation_0)
    return Copy(src);

  // TODO(emircan): Remove this when there is libyuv::I010Rotate().
  if (src.bit_depth() == PlanarYuvBuffer::BitDepth::kBitDepth10) {
    rtc::scoped_refptr<I420Buffer> aux_buffer(
        I420Buffer::Create(src.width(), src.height()));
    libyuv::I010ToI420(
        reinterpret_cast<const uint16_t*>(src.DataY()), src.StrideY() / 2,
        reinterpret_cast<const uint16_t*>(src.DataU()), src.StrideU() / 2,
        reinterpret_cast<const uint16_t*>(src.DataV()), src.StrideV() / 2,
        aux_buffer->MutableDataY(), aux_buffer->StrideY(),
        aux_buffer->MutableDataU(), aux_buffer->StrideU(),
        aux_buffer->MutableDataV(), aux_buffer->StrideV(), src.width(),
        src.height());
    rtc::scoped_refptr<I420Buffer> rotated_aux_buffer =
        Rotate(*aux_buffer, rotation);
    rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(
        rotated_aux_buffer->width(), rotated_aux_buffer->height(),
        PlanarYuvBuffer::BitDepth::kBitDepth10));
    libyuv::I420ToI010(
        rotated_aux_buffer->DataY(), rotated_aux_buffer->StrideY(),
        rotated_aux_buffer->DataU(), rotated_aux_buffer->StrideU(),
        rotated_aux_buffer->DataV(), rotated_aux_buffer->StrideV(),
        reinterpret_cast<uint16_t*>(buffer->MutableDataY()),
        buffer->StrideY() / 2,
        reinterpret_cast<uint16_t*>(buffer->MutableDataU()),
        buffer->StrideU() / 2,
        reinterpret_cast<uint16_t*>(buffer->MutableDataV()),
        buffer->StrideV() / 2, buffer->width(), buffer->height());
    return buffer;
  }
  RTC_DCHECK(src.bit_depth() == PlanarYuvBuffer::BitDepth::kBitDepth8);

  RTC_CHECK(src.DataY());
  RTC_CHECK(src.DataU());
  RTC_CHECK(src.DataV());

  int rotated_width = src.width();
  int rotated_height = src.height();
  if (rotation == webrtc::kVideoRotation_90 ||
      rotation == webrtc::kVideoRotation_270) {
    std::swap(rotated_width, rotated_height);
  }

  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      I420Buffer::Create(rotated_width, rotated_height, src.bit_depth());

  RTC_CHECK_EQ(0, libyuv::I420Rotate(
      src.DataY(), src.StrideY(),
      src.DataU(), src.StrideU(),
      src.DataV(), src.StrideV(),
      buffer->MutableDataY(), buffer->StrideY(), buffer->MutableDataU(),
      buffer->StrideU(), buffer->MutableDataV(), buffer->StrideV(),
      src.width(), src.height(),
      static_cast<libyuv::RotationMode>(rotation)));

  return buffer;
}

void I420Buffer::InitializeData() {
  memset(data_.get(), 0,
         I420DataSize(height_, stride_y_, stride_u_, stride_v_, bit_depth_));
}

int I420Buffer::width() const {
  return width_;
}

int I420Buffer::height() const {
  return height_;
}

PlanarYuvBuffer::BitDepth I420Buffer::bit_depth() const {
  return bit_depth_;
}

const uint8_t* I420Buffer::DataY() const {
  return data_.get();
}
const uint8_t* I420Buffer::DataU() const {
  return data_.get() + stride_y_ * height_;
}
const uint8_t* I420Buffer::DataV() const {
  return data_.get() + stride_y_ * height_ + stride_u_ * ((height_ + 1) / 2);
}

int I420Buffer::StrideY() const {
  return stride_y_;
}
int I420Buffer::StrideU() const {
  return stride_u_;
}
int I420Buffer::StrideV() const {
  return stride_v_;
}

uint8_t* I420Buffer::MutableDataY() {
  return const_cast<uint8_t*>(DataY());
}
uint8_t* I420Buffer::MutableDataU() {
  return const_cast<uint8_t*>(DataU());
}
uint8_t* I420Buffer::MutableDataV() {
  return const_cast<uint8_t*>(DataV());
}

// static
void I420Buffer::SetBlack(I420Buffer* buffer) {
  switch (buffer->bit_depth()) {
    case PlanarYuvBuffer::BitDepth::kBitDepth8: {
      RTC_CHECK(libyuv::I420Rect(buffer->MutableDataY(), buffer->StrideY(),
                                 buffer->MutableDataU(), buffer->StrideU(),
                                 buffer->MutableDataV(), buffer->StrideV(), 0,
                                 0, buffer->width(), buffer->height(), 0, 128,
                                 128) == 0);
      break;
    }
    // TODO(emircan): Remove this when there is libyuv::I010Rotate().
    case PlanarYuvBuffer::BitDepth::kBitDepth10: {
      rtc::scoped_refptr<I420Buffer> aux_buffer(
          I420Buffer::Create(buffer->width(), buffer->height(),
                             PlanarYuvBuffer::BitDepth::kBitDepth8));
      SetBlack(aux_buffer);
      libyuv::I420ToI010(
          aux_buffer->DataY(), aux_buffer->StrideY(), aux_buffer->DataU(),
          aux_buffer->StrideU(), aux_buffer->DataV(), aux_buffer->StrideV(),
          reinterpret_cast<uint16_t*>(buffer->MutableDataY()),
          buffer->StrideY() / 2,
          reinterpret_cast<uint16_t*>(buffer->MutableDataU()),
          buffer->StrideU() / 2,
          reinterpret_cast<uint16_t*>(buffer->MutableDataV()),
          buffer->StrideV() / 2, buffer->width(), buffer->width());
      break;
    }
  }
}

void I420Buffer::CropAndScaleFrom(const I420BufferInterface& src,
                                  int offset_x,
                                  int offset_y,
                                  int crop_width,
                                  int crop_height) {
  RTC_CHECK_LE(crop_width, src.width());
  RTC_CHECK_LE(crop_height, src.height());
  RTC_CHECK_LE(crop_width + offset_x, src.width());
  RTC_CHECK_LE(crop_height + offset_y, src.height());
  RTC_CHECK_GE(offset_x, 0);
  RTC_CHECK_GE(offset_y, 0);

  // Make sure offset is even so that u/v plane becomes aligned.
  const int uv_offset_x = offset_x / 2;
  const int uv_offset_y = offset_y / 2;
  offset_x = uv_offset_x * 2;
  offset_y = uv_offset_y * 2;

  int res = 1;
  switch (src.bit_depth()) {
    case PlanarYuvBuffer::BitDepth::kBitDepth8: {
      const uint8_t* y_plane =
          src.DataY() + src.StrideY() * offset_y + offset_x;
      const uint8_t* u_plane =
          src.DataU() + src.StrideU() * uv_offset_y + uv_offset_x;
      const uint8_t* v_plane =
          src.DataV() + src.StrideV() * uv_offset_y + uv_offset_x;
      res = libyuv::I420Scale(y_plane, src.StrideY(), u_plane, src.StrideU(),
                              v_plane, src.StrideV(), crop_width, crop_height,
                              MutableDataY(), StrideY(), MutableDataU(),
                              StrideU(), MutableDataV(), StrideV(), width(),
                              height(), libyuv::kFilterBox);
      break;
    }
    case PlanarYuvBuffer::BitDepth::kBitDepth10: {
      const uint16_t* y_plane = reinterpret_cast<const uint16_t*>(src.DataY()) +
                                (src.StrideY() / 2) * offset_y + offset_x;
      const uint16_t* u_plane = reinterpret_cast<const uint16_t*>(src.DataU()) +
                                (src.StrideU() / 2) * uv_offset_y + uv_offset_x;
      const uint16_t* v_plane = reinterpret_cast<const uint16_t*>(src.DataV()) +
                                (src.StrideV() / 2) * uv_offset_y + uv_offset_x;
      res = libyuv::I420Scale_16(
          y_plane, src.StrideY() / 2, u_plane, src.StrideU() / 2, v_plane,
          src.StrideV() / 2, crop_width, crop_height,
          reinterpret_cast<uint16_t*>(MutableDataY()), StrideY() / 2,
          reinterpret_cast<uint16_t*>(MutableDataU()), StrideU() / 2,
          reinterpret_cast<uint16_t*>(MutableDataV()), StrideV() / 2, width(),
          height(), libyuv::kFilterBox);
      break;
    }
  }
  RTC_DCHECK_EQ(res, 0);
}

void I420Buffer::CropAndScaleFrom(const I420BufferInterface& src) {
  const int crop_width =
      std::min(src.width(), width() * src.height() / height());
  const int crop_height =
      std::min(src.height(), height() * src.width() / width());

  CropAndScaleFrom(
      src,
      (src.width() - crop_width) / 2, (src.height() - crop_height) / 2,
      crop_width, crop_height);
}

void I420Buffer::ScaleFrom(const I420BufferInterface& src) {
  CropAndScaleFrom(src, 0, 0, src.width(), src.height());
}

}  // namespace webrtc
