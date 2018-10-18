/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/repeated_task.h"

namespace webrtc {

RepeatedTaskHandle::RepeatedTaskHandle(
    webrtc_repeated_task_impl::RepeatedTask* repeated_task)
    : repeated_task_(repeated_task) {}

void Stop(std::unique_ptr<webrtc::RepeatedTaskHandle> handle) {
  handle->repeated_task_->Stop();
}

}  // namespace webrtc
