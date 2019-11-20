/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_TEST_MOCK_UNDERLYING_SOURCE_H_
#define RTC_BASE_STREAMS_TEST_MOCK_UNDERLYING_SOURCE_H_

#include "rtc_base/streams/underlying_source.h"
#include "test/gmock.h"

namespace webrtc {

template <typename T>
class MockUnderlyingSource : public UnderlyingSource<T> {
 public:
  MOCK_METHOD(void, Start, (ReadableStreamController<T>*), (override));
  MOCK_METHOD(void, Pull, (ReadableStreamController<T>*), (override));
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_TEST_MOCK_UNDERLYING_SOURCE_H_
