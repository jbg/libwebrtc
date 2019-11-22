/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_READABLE_STREAM_H_
#define RTC_BASE_STREAMS_READABLE_STREAM_H_

#include <memory>
#include <utility>

#include "rtc_base/streams/pipes.h"
#include "rtc_base/streams/sink_controller.h"
#include "rtc_base/streams/source_controller.h"
#include "rtc_base/streams/underlying_source.h"
#include "rtc_base/streams/writable_stream.h"

namespace webrtc {

template <typename I, typename O>
class TransformStream;

class ReadableStreamBase : protected SourceController {
 protected:
  // SourceController implementation.
  bool IsReady() const override;
  void Start() override;
  void Pull() override;

  virtual SinkController* destination() = 0;

  virtual void StartUnderlyingSource() = 0;
  virtual void PullUnderlyingSource() = 0;

  // Called by ReadableStreamController<T>::StartAsync().
  void OnStartAsync();

  // Called by ReadableStreamController<T>::CompleteAsync().
  void OnCompleteAsync();

  // Called by ReadableStreamController<T>::Write().
  void OnWrite();

  // Called by ReadableStreamController<T>::IsWritable() if false.
  void OnBlocked() const;

 private:
  enum class State {
    kInit,
    kStarting,
    kStartPending,
    kReady,
    kPulling,
    kPullingProduced,
    kPullPending,
    kPullPendingProduced,
    kIdle,
  };

  void PokeDestination() {
    SinkController* dest = destination();
    if (dest) {
      dest->PollSource();
    }
  }

  State state_ = State::kInit;
};

template <typename T>
class ReadableStream : public ReadableStreamBase,
                       private ReadableStreamController<T> {
 public:
  explicit ReadableStream(std::unique_ptr<UnderlyingSource<T>> source)
      : source_(std::move(source)) {
    RTC_DCHECK(source_);
    Start();
  }

  ~ReadableStream() { RTC_DCHECK(!IsLocked()); }

  bool IsLocked() const { return static_cast<bool>(destination_); }

  PipeToHandle PipeTo(WritableStream<T>* destination) {
    RTC_DCHECK(destination);
    destination->Connect(this);
    return PipeToHandle(StreamConnection(destination));
  }

  template <typename O>
  PipeThroughHandle<O> PipeThrough(TransformStream<T, O>* transform) {
    RTC_DCHECK(transform);
    return PipeThroughHandle<O>(PipeTo(transform->writable()),
                                transform->readable());
  }

 private:
  friend class WritableStream<T>;

  // ReadableStreamBase implementation.
  SinkController* destination() override { return destination_; }

  void StartUnderlyingSource() override { source_->Start(this); }

  void PullUnderlyingSource() override { source_->Pull(this); }

  // ReadableStreamController implementation.
  bool IsWritable() override {
    bool ready = destination_ && destination_->IsReady();
    if (!ready) {
      OnBlocked();
    }
    return ready;
  }
  void Write(T chunk) override {
    RTC_DCHECK(IsWritable());
    OnWrite();
    if (destination_) {
      destination_->Write(std::move(chunk));
    }
  }
  void Close() override {
    RTC_DCHECK(destination_);
    destination_->Close();
  }
  void StartAsync() override { OnStartAsync(); }
  void CompleteAsync() override { OnCompleteAsync(); }

  std::unique_ptr<UnderlyingSource<T>> source_;
  WritableStream<T>* destination_ = nullptr;
};

template <typename T>
class CallbackUnderlyingSource final : public UnderlyingSource<T> {
 public:
  explicit CallbackUnderlyingSource(
      ReadableStreamController<T>** controller_out)
      : controller_out_(controller_out) {}

  // UnderlyingSource implementation.
  void Start(ReadableStreamController<T>* controller) override {
    *controller_out_ = controller;
  }
  void Pull(ReadableStreamController<T>*) override {}

 private:
  ReadableStreamController<T>** controller_out_;
};

template <typename T>
class ReadableStreamPushSource final : public ReadableStream<T> {
 public:
  ReadableStreamPushSource()
      : ReadableStream<T>(
            std::make_unique<CallbackUnderlyingSource<T>>(&controller_)) {}

  void Write(T chunk) {
    if (controller_->IsWritable()) {
      controller_->Write(std::move(chunk));
    }
  }

 private:
  ReadableStreamController<T>* controller_;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_READABLE_STREAM_H_
