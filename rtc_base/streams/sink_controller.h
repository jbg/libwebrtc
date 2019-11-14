/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_SINK_CONTROLLER_H_
#define RTC_BASE_STREAMS_SINK_CONTROLLER_H_

namespace webrtc {

class SinkController {
 public:
  virtual ~SinkController() = default;

  virtual bool IsReady() const = 0;
  virtual void Start() = 0;
  virtual void PollSource() = 0;
  virtual void Close() = 0;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_SINK_CONTROLLER_H_
