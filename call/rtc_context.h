/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RTC_CONTEXT_H_
#define CALL_RTC_CONTEXT_H_

#include "api/task_queue/task_queue_factory.h"
#include "api/transport/webrtc_key_value_config.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// Contains pointers to commonly used system functionality. The pointers can
// point to common global instances or client specific overrides. In particular,
// this struct can be used by tests to allow mocking or simulation.
struct RtcContext {
  Clock* clock;
  RtcEventLog* event_log;
  WebRtcKeyValueConfig* key_value_config;
  TaskQueueFactory* task_queue_factory;
  ProcessThreadFactory* process_thread_factory;
};
}  // namespace webrtc
#endif  // CALL_RTC_CONTEXT_H_
