/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/buffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/zero_memory.h"

namespace rtc {

TEST(ZeroMemoryTest, TestZeroMemory) {
  static const size_t kBufferSize = 32;
  uint8_t buffer[kBufferSize];
  for (size_t i = 0; i < kBufferSize; i++) {
    buffer[i] = static_cast<uint8_t>(i + 1);
  }
  ExplicitZeroMemory(buffer, sizeof(buffer));
  for (size_t i = 0; i < kBufferSize; i++) {
    EXPECT_EQ(buffer[i], 0);
  }
}

TEST(ZeroMemoryTest, TestZeroString) {
  std::string s("Hello world!");
  ExplicitZeroMemory(&s);
  for (size_t i = 0; i < s.size(); i++) {
    EXPECT_EQ(s[i], 0);
  }
}

TEST(ZeroMemoryTest, TestZeroBuffer) {
  static const size_t kBufferSize = 32;
  rtc::Buffer buffer(kBufferSize);
  for (size_t i = 0; i < kBufferSize; i++) {
    buffer[i] = static_cast<uint8_t>(i + 1);
  }
  ExplicitZeroMemory(&buffer);
  for (size_t i = 0; i < kBufferSize; i++) {
    EXPECT_EQ(buffer[i], 0);
  }
}

TEST(ZeroMemoryTest, TestZeroObject) {
  struct {
    int foo;
  } obj;
  obj.foo = 42;
  ExplicitZeroMemory(&obj);
  EXPECT_EQ(obj.foo, 0);
}

}  // namespace rtc
