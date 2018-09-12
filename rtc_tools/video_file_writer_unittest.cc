/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "rtc_base/refcountedobject.h"
#include "rtc_tools/video_file_reader.h"
#include "rtc_tools/video_file_writer.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

class Y4mVideoFileWriterTest : public ::testing::Test {
 public:
  void SetUp() override {
    const std::string filename =
        webrtc::test::OutputPath() + "test_video_file.y4m";
    const std::string filename_2 =
        webrtc::test::OutputPath() + "test_video_file2.y4m";

    // Create simple test video of size 6x4.
    FILE* file = fopen(filename.c_str(), "wb");
    ASSERT_TRUE(file != nullptr);
    fprintf(file, "YUV4MPEG2 W6 H4 F60:1 C420 dummyParam\n");
    fprintf(file, "FRAME\n");

    const int width = 6;
    const int height = 4;
    const int i40_size = width * height * 3 / 2;
    // First frame.
    for (int i = 0; i < i40_size; ++i)
      fputc(static_cast<char>(i), file);
    fprintf(file, "FRAME\n");
    // Second frame.
    for (int i = 0; i < i40_size; ++i)
      fputc(static_cast<char>(i + i40_size), file);
    fclose(file);

    // Open the newly created file.
    video = webrtc::test::OpenY4mFile(filename);
    ASSERT_TRUE(video);

    // Write and read Y4M file.
    webrtc::test::WriteVideoToFile(video, filename_2, 60);
    written_video = webrtc::test::OpenY4mFile(filename_2);
    ASSERT_TRUE(written_video);
  }
  rtc::scoped_refptr<webrtc::test::Video> video;
  rtc::scoped_refptr<webrtc::test::Video> written_video;
};

TEST_F(Y4mVideoFileWriterTest, TestParsingFileHeader) {
  EXPECT_EQ(video->width(), written_video->width());
  EXPECT_EQ(video->height(), written_video->height());
}

TEST_F(Y4mVideoFileWriterTest, TestParsingNumberOfFrames) {
  EXPECT_EQ(video->number_of_frames(), written_video->number_of_frames());
}

TEST_F(Y4mVideoFileWriterTest, TestPixelContent) {
  int cnt = 0;
  for (const rtc::scoped_refptr<I420BufferInterface> frame : *written_video) {
    for (int i = 0; i < 6 * 4; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataY()[i]);
    for (int i = 0; i < 3 * 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataU()[i]);
    for (int i = 0; i < 3 * 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataV()[i]);
  }
}

class YuvVideoFileWriterTest : public ::testing::Test {
 public:
  void SetUp() override {
    const std::string filename =
        webrtc::test::OutputPath() + "test_video_file.y4m";
    const std::string filename_yuv =
        webrtc::test::OutputPath() + "test_video_file.yuv";

    // Create simple test video of size 6x4.
    FILE* file = fopen(filename.c_str(), "wb");
    ASSERT_TRUE(file != nullptr);
    fprintf(file, "YUV4MPEG2 W6 H4 F60:1 C420 dummyParam\n");
    fprintf(file, "FRAME\n");

    const int width = 6;
    const int height = 4;
    const int i40_size = width * height * 3 / 2;
    // First frame.
    for (int i = 0; i < i40_size; ++i)
      fputc(static_cast<char>(i), file);
    fprintf(file, "FRAME\n");
    // Second frame.
    for (int i = 0; i < i40_size; ++i)
      fputc(static_cast<char>(i + i40_size), file);
    fclose(file);

    // Open the newly created file.
    video = webrtc::test::OpenY4mFile(filename);
    ASSERT_TRUE(video);

    // Write and read YUV file.
    webrtc::test::WriteVideoToFile(video, filename_yuv, 60);
    written_video = webrtc::test::OpenYuvFile(filename_yuv, width, height);
    ASSERT_TRUE(written_video);
  }
  rtc::scoped_refptr<webrtc::test::Video> video;
  rtc::scoped_refptr<webrtc::test::Video> written_video;
};

TEST_F(YuvVideoFileWriterTest, TestParsingFileHeader) {
  EXPECT_EQ(video->width(), written_video->width());
  EXPECT_EQ(video->height(), written_video->height());
}

TEST_F(YuvVideoFileWriterTest, TestParsingNumberOfFrames) {
  EXPECT_EQ(video->number_of_frames(), written_video->number_of_frames());
}

TEST_F(YuvVideoFileWriterTest, TestPixelContent) {
  int cnt = 0;
  for (const rtc::scoped_refptr<I420BufferInterface> frame : *written_video) {
    for (int i = 0; i < 6 * 4; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataY()[i]);
    for (int i = 0; i < 3 * 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataU()[i]);
    for (int i = 0; i < 3 * 2; ++i, ++cnt)
      EXPECT_EQ(cnt, frame->DataV()[i]);
  }
}

}  // namespace test
}  // namespace webrtc
