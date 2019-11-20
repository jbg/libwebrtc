/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/streams/stream.h"
#include "rtc_base/streams/test/actions.h"
#include "rtc_base/streams/test/mock_underlying_sink.h"
#include "rtc_base/streams/test/mock_underlying_source.h"
#include "rtc_base/streams/test/mock_underlying_transformer.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::WithArg;

namespace webrtc {

TEST(TransformStream, Basic) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  // EXPECT_CALL(*sink, Start(_));
  // EXPECT_CALL(*transform, Start(_));
  // EXPECT_CALL(*source, Start(_));
  EXPECT_CALL(*source, Pull(_)).WillOnce(Write(1));
  EXPECT_CALL(*transformer, Transform(1, _)).WillOnce(WithArg<1>(Write(2)));
  EXPECT_CALL(*sink, Write(2, _));

  ReadableStream<int> readable(std::move(source));
  TransformStream<int, int> transform(std::move(transformer));
  WritableStream<int> writable(std::move(sink));
  readable.PipeThrough(&transform).PipeTo(&writable);
}

}  // namespace webrtc
