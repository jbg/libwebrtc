/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/include/video_frame_buffer_pool.h"

#include <stdint.h>
#include <string.h>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "test/gtest.h"

namespace webrtc {

TEST(TestVideoFrameBufferPool, SimpleFrameReuse) {
  VideoFrameBufferPool pool;
  auto buffer = pool.CreateI420Buffer(16, 16);
  EXPECT_EQ(16, buffer->width());
  EXPECT_EQ(16, buffer->height());
  // Extract non-refcounted pointers for testing.
  const uint8_t* y_ptr = buffer->DataY();
  const uint8_t* u_ptr = buffer->DataU();
  const uint8_t* v_ptr = buffer->DataV();
  // Release buffer so that it is returned to the pool.
  buffer = nullptr;
  // Check that the memory is resued.
  buffer = pool.CreateI420Buffer(16, 16);
  EXPECT_EQ(y_ptr, buffer->DataY());
  EXPECT_EQ(u_ptr, buffer->DataU());
  EXPECT_EQ(v_ptr, buffer->DataV());
}

TEST(TestVideoFrameBufferPool, FrameReuseWithDefaultThenExplicitStride) {
  VideoFrameBufferPool pool;
  auto buffer = pool.CreateI420Buffer(15, 16);
  EXPECT_EQ(15, buffer->width());
  EXPECT_EQ(16, buffer->height());
  // The default Y stride is width and UV stride is halfwidth (rounded up).
  ASSERT_EQ(15, buffer->StrideY());
  ASSERT_EQ(8, buffer->StrideU());
  ASSERT_EQ(8, buffer->StrideV());
  // Extract non-refcounted pointers for testing.
  const uint8_t* y_ptr = buffer->DataY();
  const uint8_t* u_ptr = buffer->DataU();
  const uint8_t* v_ptr = buffer->DataV();
  // Release buffer so that it is returned to the pool.
  buffer = nullptr;
  // Check that the memory is resued with explicit strides if they match the
  // assumed default above.
  buffer = pool.CreateI420Buffer(15, 16, 15, 8, 8);
  EXPECT_EQ(y_ptr, buffer->DataY());
  EXPECT_EQ(u_ptr, buffer->DataU());
  EXPECT_EQ(v_ptr, buffer->DataV());
  EXPECT_EQ(15, buffer->width());
  EXPECT_EQ(16, buffer->height());
  EXPECT_EQ(15, buffer->StrideY());
  EXPECT_EQ(8, buffer->StrideU());
  EXPECT_EQ(8, buffer->StrideV());
}

TEST(TestVideoFrameBufferPool, FailToReuseWrongSize) {
  // Set max frames to 1, just to make sure the first buffer is being released.
  VideoFrameBufferPool pool(/*zero_initialize=*/false, 1);
  auto buffer = pool.CreateI420Buffer(16, 16);
  EXPECT_EQ(16, buffer->width());
  EXPECT_EQ(16, buffer->height());
  // Release buffer so that it is returned to the pool.
  buffer = nullptr;
  // Check that the pool doesn't try to reuse buffers of incorrect size.
  buffer = pool.CreateI420Buffer(32, 16);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(32, buffer->width());
  EXPECT_EQ(16, buffer->height());
}

TEST(TestVideoFrameBufferPool, FailToReuseWrongStride) {
  // Set max frames to 1, just to make sure the first buffer is being released.
  VideoFrameBufferPool pool(/*zero_initialize=*/false, 1);
  auto buffer = pool.CreateI420Buffer(32, 32, 32, 16, 16);
  // Make sure the stride was read correctly, for the rest of the test.
  ASSERT_EQ(16, buffer->StrideU());
  ASSERT_EQ(16, buffer->StrideV());
  buffer = pool.CreateI420Buffer(32, 32, 32, 20, 20);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(32, buffer->StrideY());
  EXPECT_EQ(20, buffer->StrideU());
  EXPECT_EQ(20, buffer->StrideV());
}

TEST(TestVideoFrameBufferPool, FrameValidAfterPoolDestruction) {
  rtc::scoped_refptr<I420Buffer> buffer;
  {
    VideoFrameBufferPool pool;
    buffer = pool.CreateI420Buffer(16, 16);
  }
  EXPECT_EQ(16, buffer->width());
  EXPECT_EQ(16, buffer->height());
  // Try to trigger use-after-free errors by writing to y-plane.
  memset(buffer->MutableDataY(), 0xA5, 16 * buffer->StrideY());
}

TEST(TestVideoFrameBufferPool, MaxNumberOfBuffers) {
  VideoFrameBufferPool pool(false, 1);
  auto buffer1 = pool.CreateI420Buffer(16, 16);
  EXPECT_NE(nullptr, buffer1.get());
  EXPECT_EQ(nullptr, pool.CreateI420Buffer(16, 16).get());
}

}  // namespace webrtc
