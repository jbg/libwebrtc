/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_TEST_MOCK_UNDERLYING_SINK_H_
#define RTC_BASE_STREAMS_TEST_MOCK_UNDERLYING_SINK_H_

#include "rtc_base/streams/underlying_sink.h"
#include "test/gmock.h"

namespace webrtc {

template <typename T>
class MockUnderlyingSink : public UnderlyingSink<T> {
 public:
  MOCK_METHOD(void, Start, (WritableStreamController<T>*), (override));
  MOCK_METHOD(void, Write, (T chunk, WritableStreamController<T>*), (override));
  MOCK_METHOD(void, Close, (WritableStreamController<T>*), (override));
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_TEST_MOCK_UNDERLYING_SINK_H_
