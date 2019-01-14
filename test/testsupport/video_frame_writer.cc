/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/video_frame_writer.h"

#include <utility>

#include "absl/memory/memory.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

VideoFrameWriter::VideoFrameWriter(std::string output_file_name,
                                   int width,
                                   int height,
                                   int fps)
    : output_file_name_(std::move(output_file_name)),
      width_(width),
      height_(height),
      fps_(fps),
      closed_(false),
      frame_writer_(absl::make_unique<Y4mFrameWriterImpl>(output_file_name_,
                                                          width_,
                                                          height_,
                                                          fps_)) {}
VideoFrameWriter::~VideoFrameWriter() = default;

void VideoFrameWriter::Init() {
  frame_writer_->Init();
}

bool VideoFrameWriter::WriteFrame(const webrtc::VideoFrame& frame) {
  {
    rtc::CritScope crit(&lock_);
    if (closed_) {
      RTC_LOG(WARNING) << "Writing to the closed file writer for file "
                       << output_file_name_;
      return false;
    }
  }
  rtc::Buffer frame_buffer = ExtractI420BufferWithSize(frame, width_, height_);
  RTC_CHECK_EQ(frame_buffer.size(), frame_writer_->FrameLength());
  return frame_writer_->WriteFrame(frame_buffer.data());
}

void VideoFrameWriter::Close() {
  rtc::CritScope crit(&lock_);
  closed_ = true;
  frame_writer_->Close();
}

rtc::Buffer VideoFrameWriter::ExtractI420BufferWithSize(const VideoFrame& frame,
                                                        int width,
                                                        int height) {
  if (frame.width() != width || frame.height() != height) {
    EXPECT_DOUBLE_EQ(static_cast<double>(width) / height,
                     static_cast<double>(frame.width()) / frame.height());
    // Same aspect ratio, no cropping needed.
    rtc::scoped_refptr<I420Buffer> scaled(I420Buffer::Create(width, height));
    scaled->ScaleFrom(*frame.video_frame_buffer()->ToI420());

    size_t length =
        CalcBufferSize(VideoType::kI420, scaled->width(), scaled->height());
    rtc::Buffer buffer(length);
    RTC_CHECK_NE(ExtractBuffer(scaled, length, buffer.data()), -1);
    return buffer;
  }

  // No resize.
  size_t length =
      CalcBufferSize(VideoType::kI420, frame.width(), frame.height());
  rtc::Buffer buffer(length);
  RTC_CHECK_NE(ExtractBuffer(frame, length, buffer.data()), -1);
  return buffer;
}

}  // namespace test
}  // namespace webrtc
