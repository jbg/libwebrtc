/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_UNITTESTS_XCTEST_TO_GTEST_H_
#define SDK_OBJC_UNITTESTS_XCTEST_TO_GTEST_H_

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

#include "test/gtest.h"

namespace webrtc {
template <typename T>
class XCTestToGTest : public ::testing::Test {
 public:
  XCTestToGTest() : test_([[T alloc] init]) {}
  ~XCTestToGTest() override = default;

 protected:
  void SetUp() override { [test_ setUp]; }

  void TearDown() override { [test_ tearDown]; }

  T* test_;
};

}  // namespace webrtc

#define INVOKE_XCTEST(fixture, name) \
  TEST_F(fixture, name) { [test_ test##name]; }

#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

#endif  // SDK_OBJC_UNITTESTS_XCTEST_TO_GTEST_H_
