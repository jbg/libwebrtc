/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_TEST_ACTIONS_H_
#define RTC_BASE_STREAMS_TEST_ACTIONS_H_

#include <utility>

#include "test/gmock.h"

namespace webrtc {

ACTION_P(StartAsync, controller_out) {
  *controller_out = arg0;
  arg0->StartAsync();
}

ACTION_P(Write, chunk) {
  arg0->Write(std::move(chunk));
}

ACTION(Close) {
  arg0->Close();
}

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_TEST_ACTIONS_H_
