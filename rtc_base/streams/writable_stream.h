/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_WRITABLE_STREAM_H_
#define RTC_BASE_STREAMS_WRITABLE_STREAM_H_

#include <memory>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/streams/sink_controller.h"
#include "rtc_base/streams/source_controller.h"
#include "rtc_base/streams/underlying_sink.h"

namespace webrtc {

template <typename T>
class ReadableStream;

class WritableStreamBase : protected SinkController {
 protected:
  // SinkController implementation.
  bool IsReady() const override;
  void Start() override;
  void PollSource() override;
  void Close() override;

  virtual void StartUnderlyingSink() = 0;
  virtual void CloseUnderlyingSink() = 0;

  virtual SourceController* origin() = 0;

  // Called by WritableStreamController<T>::StartAsync();
  void OnStartAsync();
  // Called by WritableStreamController<T>::CompleteAsync();
  void OnCompleteAsync();

  void OnWriteBegin() {
    RTC_DCHECK(state_ == State::kReady);
    state_ = State::kWriting;
  }
  void OnWriteEnd() {
    if (state_ != State::kWriting) {
      return;
    }
    state_ = State::kReady;
  }

 private:
  friend class StreamConnection;

  enum class State {
    kInit,
    kStarting,
    kStartPending,
    kStartPendingCloseRequest,
    kReady,
    kWriting,
    kWritePending,
    kWritePendingCloseRequest,
    kClosing,
    kClosePending,
    kClosed,
  };

  bool IsReentrant() {
    return state_ == State::kStarting || state_ == State::kWriting ||
           state_ == State::kClosing;
  }

  State state_ = State::kInit;
};

template <typename T>
class WritableStream final : public WritableStreamBase,
                             private WritableStreamController<T> {
 public:
  explicit WritableStream(std::unique_ptr<UnderlyingSink<T>> underlying_sink)
      : underlying_sink_(std::move(underlying_sink)) {
    RTC_DCHECK(underlying_sink_);
  }

  ~WritableStream() override { RTC_DCHECK(!IsLocked()); }

  bool IsLocked() const { return static_cast<bool>(origin_); }

 private:
  friend class ReadableStream<T>;
  template <typename O>
  friend class PipeThroughHandle;

  void Connect(ReadableStream<T>* origin) {
    RTC_DCHECK(origin);
    RTC_DCHECK(!IsLocked());
    RTC_DCHECK(!origin->IsLocked());
    origin_ = origin;
    origin_->destination_ = this;
    Start();
    origin->Start();
    PollSource();
  }

  void Write(T chunk) {
    RTC_DCHECK(IsReady());
    OnWriteBegin();
    underlying_sink_->Write(std::move(chunk), this);
    OnWriteEnd();
  }

  // WritableStreamBase overrides.
  void StartUnderlyingSink() override { underlying_sink_->Start(this); }
  void CloseUnderlyingSink() override { underlying_sink_->Close(this); }
  SourceController* origin() override { return origin_; }
  void Disconnect() override {
    RTC_DCHECK(IsLocked());
    RTC_DCHECK_EQ(origin_->destination_, this);
    origin_->destination_ = nullptr;
    origin_ = nullptr;
  }

  // WritableStreamController<T> implementation.
  void StartAsync() override { OnStartAsync(); }
  void CompleteAsync() override { OnCompleteAsync(); }

  std::unique_ptr<UnderlyingSink<T>> underlying_sink_;
  ReadableStream<T>* origin_ = nullptr;
};

template <typename T>
class CallbackUnderlyingSink final : public UnderlyingSink<T> {
 public:
  explicit CallbackUnderlyingSink(std::function<void(T)> callback)
      : callback_(std::move(callback)) {
    RTC_DCHECK(callback_);
  }

  // UnderlyingSink implementation.
  void Start(WritableStreamController<T>*) override {}
  void Write(T chunk, WritableStreamController<T>*) override {
    callback_(std::move(chunk));
  }
  void Close(WritableStreamController<T>*) override {}

 private:
  std::function<void(T)> callback_;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_WRITABLE_STREAM_H_
