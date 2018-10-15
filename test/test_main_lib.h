/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TEST_MAIN_LIB_H_
#define TEST_TEST_MAIN_LIB_H_

#include <memory>

namespace webrtc {

class TestMain {
 public:
  virtual ~TestMain() {}

  static std::unique_ptr<TestMain> Create();

  // Initialize test environment. Returns 0 on success and error code otherwise.
  // After initialization user can run own required code.
  virtual int Init(int argc, char* argv[]) = 0;

  // Runs test end return result error code. 0 - no errors.
  virtual int Run(int argc, char* argv[]) = 0;
};

}  // namespace webrtc

#endif  // TEST_TEST_MAIN_LIB_H_
