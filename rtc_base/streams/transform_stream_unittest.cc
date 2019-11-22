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
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::SaveArg;

namespace webrtc {

TEST(TransformStream, TransformerStartWhenTransformConstructed) {
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, bool>>();
  EXPECT_CALL(*transformer, Start(_));
  TransformStream<int, bool> transform(std::move(transformer));
}

TEST(TransformStream, Basic) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  EXPECT_CALL(*source, Pull(_)).WillOnce(Write(1));
  EXPECT_CALL(*transformer, Transform(1, _)).WillOnce(Write(2));
  EXPECT_CALL(*sink, Write(2, _));
  EXPECT_CALL(*source, Pull(_));

  ReadableStream<int> readable(std::move(source));
  TransformStream<int, int> transform(std::move(transformer));
  WritableStream<int> writable(std::move(sink));
  readable.PipeThrough(&transform).PipeTo(&writable);
}

TEST(TransformStream, ManualPipeToWritable) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, int>>();

  InSequence seq;
  EXPECT_CALL(*source, Start(_));
  EXPECT_CALL(*source, Pull(_)).Times(0);
  EXPECT_CALL(*transformer, Transform(_, _)).Times(0);

  ReadableStream<int> readable(std::move(source));
  TransformStream<int, int> transform(std::move(transformer));
  readable.PipeTo(transform.writable());
}

TEST(TransformStream, ManualPipeToReadable) {
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  EXPECT_CALL(*transformer, Start(_))
      .WillOnce(Invoke([](TransformStreamController<int>* controller) {
        EXPECT_FALSE(controller->IsWritable());
      }));
  EXPECT_CALL(*sink, Start(_));

  TransformStream<int, int> transform(std::move(transformer));
  WritableStream<int> writable(std::move(sink));
  transform.readable()->PipeTo(&writable);
}

TEST(TransformStream, SyncTransformAsyncSink) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, bool>>();
  auto sink = std::make_unique<MockUnderlyingSink<bool>>();

  MockFunction<void(const char*)> check;
  InSequence seq;
  EXPECT_CALL(*source, Pull(_)).WillOnce(Write(1));
  EXPECT_CALL(*transformer, Transform(1, _)).WillOnce(Write(false));
  WritableStreamController<bool>* writable_controller;
  EXPECT_CALL(*sink, Write(false, _))
      .WillOnce(StartAsync(&writable_controller));
  EXPECT_CALL(check, Call("CompleteAsync"));
  EXPECT_CALL(*source, Pull(_)).WillOnce(Write(2));
  EXPECT_CALL(*transformer, Transform(2, _)).WillOnce(Write(true));
  EXPECT_CALL(*sink, Write(true, _));
  EXPECT_CALL(*source, Pull(_));

  ReadableStream<int> readable(std::move(source));
  TransformStream<int, bool> transform(std::move(transformer));
  WritableStream<bool> writable(std::move(sink));
  auto handle = readable.PipeThrough(&transform).PipeTo(&writable);

  check.Call("CompleteAsync");
  writable_controller->CompleteAsync();
}

TEST(TransformStream, SyncTransformPushSource) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, bool>>();
  auto sink = std::make_unique<MockUnderlyingSink<bool>>();

  InSequence seq;
  ReadableStreamController<int>* readable_controller;
  EXPECT_CALL(*source, Start(_)).WillOnce(SaveArg<0>(&readable_controller));
  EXPECT_CALL(*transformer, Transform(1, _)).WillOnce(Write(false));
  EXPECT_CALL(*sink, Write(false, _));
  EXPECT_CALL(*transformer, Transform(2, _)).WillOnce(Write(true));
  EXPECT_CALL(*sink, Write(true, _));

  ReadableStream<int> readable(std::move(source));
  TransformStream<int, bool> transform(std::move(transformer));
  WritableStream<bool> writable(std::move(sink));
  auto handle = readable.PipeThrough(&transform).PipeTo(&writable);

  readable_controller->Write(1);
  readable_controller->Write(2);
}

TEST(TransformStream, AsyncTransform) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, bool>>();
  auto sink = std::make_unique<MockUnderlyingSink<bool>>();

  InSequence seq;
  EXPECT_CALL(*source, Pull(_)).WillOnce(Write(1));
  TransformStreamController<bool>* transform_controller;
  EXPECT_CALL(*transformer, Transform(1, _))
      .WillOnce(StartAsync(&transform_controller));
  EXPECT_CALL(*sink, Write(false, _));
  EXPECT_CALL(*source, Pull(_));

  ReadableStream<int> readable(std::move(source));
  TransformStream<int, bool> transform(std::move(transformer));
  WritableStream<bool> writable(std::move(sink));
  auto handle = readable.PipeThrough(&transform).PipeTo(&writable);

  transform_controller->Write(false);
  transform_controller->CompleteAsync();
}

TEST(TransformStream, StartSinkAsync) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto transformer = std::make_unique<MockUnderlyingTransformer<int, bool>>();
  auto sink = std::make_unique<MockUnderlyingSink<bool>>();

  MockFunction<void(const char*)> check;
  InSequence seq;
  WritableStreamController<bool>* writable_controller;
  EXPECT_CALL(*sink, Start(_)).WillOnce(StartAsync(&writable_controller));
  EXPECT_CALL(check, Call("CompleteAsync"));
  EXPECT_CALL(*source, Pull(_));

  ReadableStream<int> readable(std::move(source));
  TransformStream<int, bool> transform(std::move(transformer));
  WritableStream<bool> writable(std::move(sink));
  auto handle = readable.PipeThrough(&transform).PipeTo(&writable);

  check.Call("CompleteAsync");
  writable_controller->CompleteAsync();
}

}  // namespace webrtc
