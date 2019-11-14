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
#include "test/gtest.h"

namespace webrtc {
namespace {

class FiniteSyncPullSource : public UnderlyingSource<int> {
 public:
  explicit FiniteSyncPullSource(std::vector<int> vals)
      : vals_(std::move(vals)) {}

  // UnderlyingSource implementation.
  void Start(ReadableStreamController<int>*) override {}

  void Pull(ReadableStreamController<int>* controller) override {
    if (i_ < vals_.size()) {
      controller->Enqueue(vals_[i_++]);
    }
    if (i_ >= vals_.size()) {
      controller->Close();
    }
  }

 private:
  std::vector<int> vals_;
  size_t i_ = 0;
};

template <typename T>
class SyncRecordingSink;

template <typename T>
class SinkRecording final {
 public:
  bool IsStarted() const { return state_ == State::kStarted; }

  bool IsClosed() const { return state_ == State::kClosed; }

  rtc::ArrayView<const T> chunks() const { return chunks_; }

 private:
  friend class SyncRecordingSink<T>;

  void OnStart() {
    RTC_DCHECK(state_ == State::kInit);
    state_ = State::kStarted;
  }

  void OnWrite(T chunk) {
    RTC_DCHECK(state_ == State::kStarted);
    chunks_.push_back(std::move(chunk));
  }

  void OnClose() {
    RTC_DCHECK(state_ == State::kStarted);
    state_ = State::kClosed;
  }

  enum class State { kInit, kStarted, kClosed };

  State state_ = State::kInit;
  std::vector<T> chunks_;
};

template <typename T>
class SyncRecordingSink final : public UnderlyingSink<T> {
 public:
  explicit SyncRecordingSink(SinkRecording<T>* recording)
      : recording_(recording) {
    RTC_DCHECK(recording_);
  }

  // UnderlyingSink implementation.
  void Start(WritableStreamController<T>*) override { recording_->OnStart(); }
  void Write(T chunk, WritableStreamController<T>*) override {
    recording_->OnWrite(std::move(chunk));
  }
  void Close(WritableStreamController<T>*) override { recording_->OnClose(); }

 private:
  SinkRecording<T>* const recording_;
};

}  // namespace

TEST(Streams, Basic) {
  ReadableStream<int> readable(
      std::make_unique<FiniteSyncPullSource>(std::vector<int>{1, 2, 3}));
  SinkRecording<int> recording;
  WritableStream<int> writable(
      std::make_unique<SyncRecordingSink<int>>(&recording));

  auto handle = readable.PipeTo(&writable);

  EXPECT_TRUE(recording.IsClosed());
}

}  // namespace webrtc
