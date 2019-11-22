/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/array_view.h"
#include "rtc_base/streams/stream.h"
#include "rtc_base/streams/test/actions.h"
#include "rtc_base/streams/test/mock_underlying_sink.h"
#include "rtc_base/streams/test/mock_underlying_source.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::SaveArg;

namespace webrtc {

TEST(Streams, SourceStartWhenReadableConstructed) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  EXPECT_CALL(*source, Start(_));
  ReadableStream<int> readable(std::move(source));
}

TEST(Streams, SinkStartWhenWritableConstructed) {
  auto sink = std::make_unique<MockUnderlyingSink<int>>();
  EXPECT_CALL(*sink, Start(_));
  WritableStream<int> writable(std::move(sink));
}

TEST(Streams, BasicSyncPull) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  EXPECT_CALL(*source, Pull(_)).WillOnce(Write(1));
  EXPECT_CALL(*sink, Write(1, _));
  EXPECT_CALL(*source, Pull(_)).WillOnce(DoAll(Write(2), Close()));
  EXPECT_CALL(*sink, Write(2, _));
  EXPECT_CALL(*sink, Close(_));

  ReadableStream<int> readable(std::move(source));
  WritableStream<int> writable(std::move(sink));
  readable.PipeTo(&writable);
}

TEST(Streams, BasicSyncPush) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  EXPECT_CALL(*source, Pull(_)).WillOnce(DoAll(Write(1), Write(2), Close()));
  EXPECT_CALL(*sink, Write(1, _));
  EXPECT_CALL(*sink, Write(2, _));
  EXPECT_CALL(*sink, Close(_));

  ReadableStream<int> readable(std::move(source));
  WritableStream<int> writable(std::move(sink));
  readable.PipeTo(&writable);
}

TEST(Streams, BasicAsyncPush) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  ReadableStreamController<int>* readable_controller = nullptr;
  EXPECT_CALL(*source, Start(_)).WillOnce(SaveArg<0>(&readable_controller));
  EXPECT_CALL(*sink, Write(1, _));
  EXPECT_CALL(*sink, Close(_));

  ReadableStream<int> readable(std::move(source));
  WritableStream<int> writable(std::move(sink));
  auto handle = readable.PipeTo(&writable);

  readable_controller->Write(1);
  readable_controller->Close();
}

TEST(Streams, SinkStartBackPressure) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  ReadableStreamController<int>* readable_controller = nullptr;
  EXPECT_CALL(*source, Start(_)).WillOnce(SaveArg<0>(&readable_controller));

  WritableStreamController<int>* writable_controller = nullptr;
  EXPECT_CALL(*sink, Start(_)).WillOnce(StartAsync(&writable_controller));

  MockFunction<void(const char*)> check;
  InSequence seq;
  EXPECT_CALL(check, Call("Start CompleteAsync"));
  EXPECT_CALL(*source, Pull(_))
      .WillOnce(Invoke([](ReadableStreamController<int>* controller) {
        EXPECT_TRUE(controller->IsWritable());
        controller->Write(1);
        controller->Close();
      }));
  EXPECT_CALL(*sink, Write(1, _));
  EXPECT_CALL(*sink, Close(_));

  ReadableStream<int> readable(std::move(source));
  WritableStream<int> writable(std::move(sink));
  auto handle = readable.PipeTo(&writable);

  EXPECT_FALSE(readable_controller->IsWritable());
  check.Call("Start CompleteAsync");
  writable_controller->CompleteAsync();
}

TEST(Streams, SourceStartBackPressure) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  ReadableStreamController<int>* readable_controller = nullptr;
  EXPECT_CALL(*source, Start(_)).WillOnce(StartAsync(&readable_controller));
  bool pulled = false;
  EXPECT_CALL(*source, Pull(_)).WillOnce(Assign(&pulled, true));

  ReadableStream<int> readable(std::move(source));
  WritableStream<int> writable(std::move(sink));
  auto handle = readable.PipeTo(&writable);

  EXPECT_FALSE(pulled);

  readable_controller->CompleteAsync();
}

TEST(Streams, PushSourceBackPressure) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  ReadableStreamController<int>* readable_controller = nullptr;
  EXPECT_CALL(*source, Start(_)).WillOnce(SaveArg<0>(&readable_controller));
  EXPECT_CALL(*source, Pull(_));
  WritableStreamController<int>* writable_controller = nullptr;
  EXPECT_CALL(*sink, Write(1, _)).WillOnce(StartAsync(&writable_controller));
  EXPECT_CALL(*source, Pull(_)).WillOnce(Write(2));
  EXPECT_CALL(*sink, Write(2, _));
  EXPECT_CALL(*source, Pull(_));

  ReadableStream<int> readable(std::move(source));
  WritableStream<int> writable(std::move(sink));
  auto handle = readable.PipeTo(&writable);

  EXPECT_TRUE(readable_controller->IsWritable());
  readable_controller->Write(1);

  EXPECT_FALSE(readable_controller->IsWritable());
  writable_controller->CompleteAsync();
}

}  // namespace webrtc
