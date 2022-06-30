/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/task_queue_libevent.h"
#include "rtc_base/task_queue_stdlib.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

std::unique_ptr<TaskQueueFactory> CreateDefaultTaskQueueFactory() {
  if (field_trial::IsEnabled("WebRTC-TaskQueue-ReplaceLibeventWithStdlib"))
    return CreateTaskQueueStdlibFactory();
  else
    return CreateTaskQueueLibeventFactory();
}

}  // namespace webrtc
