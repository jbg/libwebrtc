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
using ::testing::SaveArg;

namespace webrtc {
namespace {

ACTION_P(StartAsync, controller_out) {
  *controller_out = arg0;
  arg0->StartAsync();
}

ACTION_P(Write, chunk) {
  arg0->Write(std::move(chunk));
}

ACTION(Close) {
  arg0->Close();
}

}  // namespace

TEST(Streams, BasicSyncPull) {
  auto source = std::make_unique<MockUnderlyingSource<int>>();
  auto sink = std::make_unique<MockUnderlyingSink<int>>();

  InSequence seq;
  EXPECT_CALL(*sink, Start(_));
  EXPECT_CALL(*source, Start(_));
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
  EXPECT_CALL(*sink, Start(_));
  EXPECT_CALL(*source, Start(_)).WillOnce(DoAll(Write(1), Write(2), Close()));
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

  InSequence seq;

  WritableStreamController<int>* writable_controller = nullptr;
  EXPECT_CALL(*sink, Start(_)).WillOnce(StartAsync(&writable_controller));

  ReadableStreamController<int>* readable_controller = nullptr;
  EXPECT_CALL(*source, Start(_)).WillOnce(SaveArg<0>(&readable_controller));

  bool pulled = false;
  EXPECT_CALL(*source, Pull(_))
      .WillOnce(DoAll(Assign(&pulled, true),
                      Invoke([](ReadableStreamController<int>* controller) {
                        EXPECT_TRUE(controller->IsWritable());
                        controller->Write(1);
                        controller->Close();
                      })));

  EXPECT_CALL(*sink, Write(1, _));
  EXPECT_CALL(*sink, Close(_));

  ReadableStream<int> readable(std::move(source));
  WritableStream<int> writable(std::move(sink));

  auto handle = readable.PipeTo(&writable);

  EXPECT_FALSE(readable_controller->IsWritable());
  EXPECT_FALSE(pulled);

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

}  // namespace webrtc
