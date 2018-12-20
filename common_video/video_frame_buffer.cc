/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "common_video/include/video_frame_buffer.h"

#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace webrtc {

namespace {

// Template to implement a wrapped buffer for a I4??BufferInterface.
template <typename Base>
class WrappedYuvBuffer : public Base {
 public:
  WrappedYuvBuffer(int width,
                   int height,
                   const uint8_t* y_plane,
                   int y_stride,
                   const uint8_t* u_plane,
                   int u_stride,
                   const uint8_t* v_plane,
                   int v_stride,
                   const rtc::Callback0<void>& no_longer_used)
      : width_(width),
        height_(height),
        y_plane_(y_plane),
        u_plane_(u_plane),
        v_plane_(v_plane),
        y_stride_(y_stride),
        u_stride_(u_stride),
        v_stride_(v_stride),
        no_longer_used_cb_(no_longer_used) {}

  ~WrappedYuvBuffer() override { no_longer_used_cb_(); }

  int width() const override { return width_; }

  int height() const override { return height_; }

  const uint8_t* DataY() const override { return y_plane_; }

  const uint8_t* DataU() const override { return u_plane_; }

  const uint8_t* DataV() const override { return v_plane_; }

  int StrideY() const override { return y_stride_; }

  int StrideU() const override { return u_stride_; }

  int StrideV() const override { return v_stride_; }

 private:
  friend class rtc::RefCountedObject<WrappedYuvBuffer>;

  const int width_;
  const int height_;
  const uint8_t* const y_plane_;
  const uint8_t* const u_plane_;
  const uint8_t* const v_plane_;
  const int y_stride_;
  const int u_stride_;
  const int v_stride_;
  rtc::Callback0<void> no_longer_used_cb_;
};

// Template to implement a wrapped buffer for a I4??BufferInterface.
template <typename BaseWithA>
class WrappedYuvaBuffer : public WrappedYuvBuffer<BaseWithA> {
 public:
  WrappedYuvaBuffer(int width,
                    int height,
                    const uint8_t* y_plane,
                    int y_stride,
                    const uint8_t* u_plane,
                    int u_stride,
                    const uint8_t* v_plane,
                    int v_stride,
                    const uint8_t* a_plane,
                    int a_stride,
                    const rtc::Callback0<void>& no_longer_used)
      : WrappedYuvBuffer<BaseWithA>(width,
                                    height,
                                    y_plane,
                                    y_stride,
                                    u_plane,
                                    u_stride,
                                    v_plane,
                                    v_stride,
                                    no_longer_used),
        a_plane_(a_plane),
        a_stride_(a_stride) {}

  const uint8_t* DataA() const override { return a_plane_; }
  int StrideA() const override { return a_stride_; }

 private:
  const uint8_t* const a_plane_;
  const int a_stride_;
};

class I444BufferBase : public I444BufferInterface {
 public:
  rtc::scoped_refptr<I420BufferInterface> ToI420() final;
};

rtc::scoped_refptr<I420BufferInterface> I444BufferBase::ToI420() {
  rtc::scoped_refptr<I420Buffer> i420_buffer =
      I420Buffer::Create(width(), height());
  libyuv::I444ToI420(DataY(), StrideY(), DataU(), StrideU(), DataV(), StrideV(),
                     i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                     i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                     i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                     width(), height());
  return i420_buffer;
}

// Template to implement a wrapped buffer for a PlanarYuv16BBuffer.
template <typename Base>
class WrappedYuv16BBuffer : public Base {
 public:
  WrappedYuv16BBuffer(int width,
                      int height,
                      const uint16_t* y_plane,
                      int y_stride,
                      const uint16_t* u_plane,
                      int u_stride,
                      const uint16_t* v_plane,
                      int v_stride,
                      const rtc::Callback0<void>& no_longer_used)
      : width_(width),
        height_(height),
        y_plane_(y_plane),
        u_plane_(u_plane),
        v_plane_(v_plane),
        y_stride_(y_stride),
        u_stride_(u_stride),
        v_stride_(v_stride),
        no_longer_used_cb_(no_longer_used) {}

  ~WrappedYuv16BBuffer() override { no_longer_used_cb_(); }

  int width() const override { return width_; }

  int height() const override { return height_; }

  const uint16_t* DataY() const override { return y_plane_; }

  const uint16_t* DataU() const override { return u_plane_; }

  const uint16_t* DataV() const override { return v_plane_; }

  int StrideY() const override { return y_stride_; }

  int StrideU() const override { return u_stride_; }

  int StrideV() const override { return v_stride_; }

 private:
  friend class rtc::RefCountedObject<WrappedYuv16BBuffer>;

  const int width_;
  const int height_;
  const uint16_t* const y_plane_;
  const uint16_t* const u_plane_;
  const uint16_t* const v_plane_;
  const int y_stride_;
  const int u_stride_;
  const int v_stride_;
  rtc::Callback0<void> no_longer_used_cb_;
};

class I010BufferBase : public I010BufferInterface {
 public:
  rtc::scoped_refptr<I420BufferInterface> ToI420() final;
};

rtc::scoped_refptr<I420BufferInterface> I010BufferBase::ToI420() {
  rtc::scoped_refptr<I420Buffer> i420_buffer =
      I420Buffer::Create(width(), height());
  libyuv::I010ToI420(DataY(), StrideY(), DataU(), StrideU(), DataV(), StrideV(),
                     i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                     i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                     i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                     width(), height());
  return i420_buffer;
}

template <typename PlanarBufferType>
void CopyYUV(PlanarBufferType* canvas,
             const PlanarBufferType* picture,
             int offset_row,
             int offset_col) {
  const int chroma_scale = picture->ChromaWidth() == picture->width() ? 1 : 2;
  RTC_DCHECK_EQ(picture->width() + picture->width() % chroma_scale,
                picture->ChromaWidth() * chroma_scale);

  typedef typename PlanarBufferType::DataType DataType;

  for (int row = 0; row < picture->height(); ++row) {
    DataType* canvas_y = const_cast<DataType*>(canvas->DataY()) +
                         canvas->StrideY() * (offset_row + row) + offset_col;
    const DataType* picture_y = picture->DataY() + picture->StrideY() * row;
    memcpy(canvas_y, picture_y, picture->width() * sizeof(DataType));
  }
  offset_row /= chroma_scale;
  offset_col /= chroma_scale;
  for (int row = 0; row < picture->ChromaHeight(); ++row) {
    DataType* canvas_v = const_cast<DataType*>(canvas->DataV()) +
                         canvas->StrideV() * (offset_row + row) + offset_col;
    const DataType* picture_v = picture->DataV() + picture->StrideV() * row;
    DataType* canvas_u = const_cast<DataType*>(canvas->DataU()) +
                         canvas->StrideU() * (offset_row + row) + offset_col;
    const DataType* picture_u = picture->DataU() + picture->StrideU() * row;
    memcpy(canvas_u, picture_u, picture->ChromaWidth() * sizeof(DataType));
    memcpy(canvas_v, picture_v, picture->ChromaWidth() * sizeof(DataType));
  }
}

}  // namespace

rtc::scoped_refptr<I420BufferInterface> WrapI420Buffer(
    int width,
    int height,
    const uint8_t* y_plane,
    int y_stride,
    const uint8_t* u_plane,
    int u_stride,
    const uint8_t* v_plane,
    int v_stride,
    const rtc::Callback0<void>& no_longer_used) {
  return rtc::scoped_refptr<I420BufferInterface>(
      new rtc::RefCountedObject<WrappedYuvBuffer<I420BufferInterface>>(
          width, height, y_plane, y_stride, u_plane, u_stride, v_plane,
          v_stride, no_longer_used));
}

rtc::scoped_refptr<I420ABufferInterface> WrapI420ABuffer(
    int width,
    int height,
    const uint8_t* y_plane,
    int y_stride,
    const uint8_t* u_plane,
    int u_stride,
    const uint8_t* v_plane,
    int v_stride,
    const uint8_t* a_plane,
    int a_stride,
    const rtc::Callback0<void>& no_longer_used) {
  return rtc::scoped_refptr<I420ABufferInterface>(
      new rtc::RefCountedObject<WrappedYuvaBuffer<I420ABufferInterface>>(
          width, height, y_plane, y_stride, u_plane, u_stride, v_plane,
          v_stride, a_plane, a_stride, no_longer_used));
}

rtc::scoped_refptr<I444BufferInterface> WrapI444Buffer(
    int width,
    int height,
    const uint8_t* y_plane,
    int y_stride,
    const uint8_t* u_plane,
    int u_stride,
    const uint8_t* v_plane,
    int v_stride,
    const rtc::Callback0<void>& no_longer_used) {
  return rtc::scoped_refptr<I444BufferInterface>(
      new rtc::RefCountedObject<WrappedYuvBuffer<I444BufferBase>>(
          width, height, y_plane, y_stride, u_plane, u_stride, v_plane,
          v_stride, no_longer_used));
}

rtc::scoped_refptr<PlanarYuvBuffer> WrapYuvBuffer(
    VideoFrameBuffer::Type type,
    int width,
    int height,
    const uint8_t* y_plane,
    int y_stride,
    const uint8_t* u_plane,
    int u_stride,
    const uint8_t* v_plane,
    int v_stride,
    const rtc::Callback0<void>& no_longer_used) {
  switch (type) {
    case VideoFrameBuffer::Type::kI420:
      return WrapI420Buffer(width, height, y_plane, y_stride, u_plane, u_stride,
                            v_plane, v_stride, no_longer_used);
    case VideoFrameBuffer::Type::kI444:
      return WrapI444Buffer(width, height, y_plane, y_stride, u_plane, u_stride,
                            v_plane, v_stride, no_longer_used);
    default:
      FATAL() << "Unexpected frame buffer type.";
      return nullptr;
  }
}

rtc::scoped_refptr<I010BufferInterface> WrapI010Buffer(
    int width,
    int height,
    const uint16_t* y_plane,
    int y_stride,
    const uint16_t* u_plane,
    int u_stride,
    const uint16_t* v_plane,
    int v_stride,
    const rtc::Callback0<void>& no_longer_used) {
  return rtc::scoped_refptr<I010BufferInterface>(
      new rtc::RefCountedObject<WrappedYuv16BBuffer<I010BufferBase>>(
          width, height, y_plane, y_stride, u_plane, u_stride, v_plane,
          v_stride, no_longer_used));
}

bool PasteIntoBuffer(VideoFrameBuffer* canvas,
                     const VideoFrameBuffer& picture,
                     int offset_col,
                     int offset_row) {
  if (canvas->type() != picture.type()) {
    RTC_LOG(LS_ERROR) << "Can't paste into a buffer of different type.";
    return false;
  }
  if (picture.type() == VideoFrameBuffer::Type::kNative) {
    RTC_LOG(LS_ERROR) << "Can't paste into a Native buffer.";
    return false;
  }

  if (picture.width() + offset_col > canvas->width() ||
      picture.height() + offset_row > canvas->height() || offset_col < 0 ||
      offset_row < 0) {
    RTC_LOG(LS_ERROR) << "No space in the canvas to paste the picture to";
    return false;
  }

  if ((picture.type() == VideoFrameBuffer::Type::kI420 ||
       picture.type() == VideoFrameBuffer::Type::kI420A ||
       picture.type() == VideoFrameBuffer::Type::kI010) &&
      (offset_col % 2 != 0 || offset_row % 2 != 0 || picture.width() % 2 != 0 ||
       picture.height() % 2 != 0)) {
    RTC_LOG(LS_ERROR) << "Can't paste unaligned picture to I420 buffer.";
    return false;
  }

  switch (picture.type()) {
    case VideoFrameBuffer::Type::kNative:
      break;
    case VideoFrameBuffer::Type::kI420: {
      CopyYUV<PlanarYuv8Buffer>(canvas->GetI420(), picture.GetI420(),
                                offset_row, offset_col);
    } break;
    case VideoFrameBuffer::Type::kI420A: {
      I420ABufferInterface* canvas_ptr = canvas->GetI420A();
      const I420ABufferInterface* picture_ptr = picture.GetI420A();

      for (int row = 0; row < picture_ptr->height(); ++row) {
        uint8_t* canvas_a = const_cast<uint8_t*>(canvas_ptr->DataA()) +
                            canvas_ptr->StrideA() * (offset_row + row) +
                            offset_col;
        const uint8_t* picture_a =
            picture_ptr->DataA() + picture_ptr->StrideA() * row;
        memcpy(canvas_a, picture_a, picture_ptr->width());
      }

      CopyYUV<PlanarYuv8Buffer>(canvas_ptr, picture_ptr, offset_row,
                                offset_col);
    } break;
    case VideoFrameBuffer::Type::kI444: {
      CopyYUV<PlanarYuv8Buffer>(canvas->GetI444(), picture.GetI444(),
                                offset_row, offset_col);
    } break;
    case VideoFrameBuffer::Type::kI010: {
      CopyYUV<PlanarYuv16BBuffer>(canvas->GetI010(), picture.GetI010(),
                                  offset_row, offset_col);
    } break;
  }
  return true;
}

}  // namespace webrtc
