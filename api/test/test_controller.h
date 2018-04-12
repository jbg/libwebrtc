/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_TEST_CONTROLLER_H_
#define API_TEST_TEST_CONTROLLER_H_

#include <memory>
#include <vector>

namespace webrtc {

class TestControllerInterface {
 public:
  virtual ~TestControllerInterface() = default;

  virtual std::unique_ptr<FecController> CreateFecController() = 0;
  virtual std::unique_ptr<test::LayerFilteringTransport>
      CreateSendTransport() = 0;
  virtual std::unique_ptr<test::DirectTransport> CreateReceiveTransport() = 0;
};

}  // namespace webrtc

#endif  // API_TEST_TEST_CONTROLLER_H_
