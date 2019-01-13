/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_VIDEO_FRAME_WRITER_H_
#define TEST_TESTSUPPORT_VIDEO_FRAME_WRITER_H_

#include <memory>
#include <string>

#include "api/video/video_frame.h"
#include "rtc_base/buffer.h"
#include "rtc_base/critical_section.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace test {

// Writes webrtc::VideoFrame to specified file with y4m frame writer
class VideoFrameWriter {
 public:
  VideoFrameWriter(const std::string& output_file_name,
                   int width,
                   int height,
                   int fps);
  virtual ~VideoFrameWriter();

  void Init();
  bool WriteFrame(const webrtc::VideoFrame& frame);
  void Close();

 private:
  rtc::Buffer ExtractI420BufferWithSize(const VideoFrame& image,
                                        int width,
                                        int height);

  const std::string output_file_name_;
  const int width_;
  const int height_;
  const int fps_;

  rtc::CriticalSection lock_;
  bool closed_ RTC_GUARDED_BY(lock_);

  std::unique_ptr<FrameWriter> frame_writer_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_VIDEO_FRAME_WRITER_H_
