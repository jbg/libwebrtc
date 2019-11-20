/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_UNDERLYING_SINK_H_
#define RTC_BASE_STREAMS_UNDERLYING_SINK_H_

namespace webrtc {

template <typename T>
class WritableStreamController {
 public:
  virtual ~WritableStreamController() = default;

  virtual void StartAsync() = 0;

  virtual void CompleteAsync() = 0;
};

template <typename T>
class UnderlyingSink {
 public:
  virtual ~UnderlyingSink() = default;

  virtual void Start(WritableStreamController<T>*) = 0;

  virtual void Write(T chunk, WritableStreamController<T>*) = 0;

  virtual void Close(WritableStreamController<T>*) = 0;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_UNDERLYING_SINK_H_
