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

#include "rtc_base/streams/underlying_source.h"
#include "rtc_base/streams/writable_stream.h"

namespace webrtc {

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

class ReadableStreamBase : protected SourceController {
 protected:
  friend struct PipeToHandle;

  virtual void Disconnect() = 0;

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

  // SourceController implementation.
  bool IsReady() const override;
  void Start() override;
  void Pull() override;

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

struct PipeThroughHandleBase {
 public:
  virtual ~PipeThroughHandleBase() = default;
};

struct PipeToHandle final {
 public:
  PipeToHandle() = default;

  explicit PipeToHandle(ReadableStreamBase* readable) : readable_(readable) {
    RTC_DCHECK(readable_);
  }

  PipeToHandle(ReadableStreamBase* readable, std::unique_ptr<PipeThroughHandle> pipe_through) : readable_(readable), pipe_through_(std::move(pipe_through)) {
    RTC_DCHECK(readable_);
    RTC_DCHECK(pipe_through_);
  }

  PipeToHandle(const PipeToHandle&) = delete;
  PipeToHandle(PipeToHandle&& o) { std::swap(readable_, o.readable_); }

  PipeToHandle& operator=(const PipeToHandle&) = delete;
  PipeToHandle& operator=(PipeToHandle&& o) {
    Release();
    std::swap(readable_, o.readable_);
    return *this;
  }

  ~PipeToHandle() { Release(); }

  void Release() {
    if (readable_) {
      readable_->Release();
      readable_ = nullptr;
    }
    pipe_through_.reset();
  }

  operator bool() const {
    return static_cast<bool>(readable_);
  }

 private:
  ReadableStreamBase* readable_ = nullptr;
  std::unique_ptr<PipeToHandle> upstream_;
};

template <typename O>
struct PipeThroughHandle final {
 public:
  PipeThroughHandle(PipeToHandle input_pipe, ReadableStream<O>* readable) : input_pipe_(std::move(input_pipe)), readable_(readable) {
    RTC_DCHECK(input_pipe_);
    RTC_DCHECK(readable_);
  }

  PipeToHandle PipeTo(WritableStream<T>* destination) && {
    return Pipereadable_->PipeTo(destination);
  }

 private:
  PipeToHandle input_pipe_;
  ReadableStream<O>* readable_;
};

template <typename T>
class ReadableStream : public ReadableStreamBase,
                       private ReadableStreamController<T> {
 public:
  explicit ReadableStream(std::unique_ptr<UnderlyingSource<T>> source)
      : source_(std::move(source)) {
    RTC_DCHECK(source_);
  }

  void Connect(WritableStream<T>* destination) {
    RTC_DCHECK(destination);
    RTC_DCHECK(!IsLocked());
    RTC_DCHECK(!destination->IsLocked());
    destination_ = destination;
    destination->origin_ = this;
    destination->Start();
    Start();
    destination->PollSource();
  }

  PipeToHandle PipeTo(WritableStream<T>* destination) {
    return PipeToHandle{this};
  }

  bool IsLocked() { return static_cast<bool>(destination_); }

  template <typename O>
  PipeThroughHandle PipeThrough(TransformStream<T, O>* transform) {
    PipeToHandle input_pipe = PipeTo(transform->writable());
    return PipeThroughHandle(std::move(input_pipe), transform->readable());
  }

 private:
  friend class WritableStream<T>;

  // ReadableStreamBase implementation.
  void Release() override {
    RTC_DCHECK(destination_);
    destination_->origin_ = nullptr;
    destination_ = nullptr;
  }

  SinkController* destination() override { return destination_; }

  void StartUnderlyingSource() override { source_->Start(this); }

  void PullUnderlyingSource() override { source_->Pull(this); }

  // ReadableStreamController implementation.
  bool IsWritable() const override {
    bool ready = destination_ && destination_->IsReady();
    if (!ready) {
      OnBlocked();
    }
    return ready;
  }
  void Write(T chunk) override {
    OnWrite();
    if (destination_ && destination_->IsReady()) {
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
class ReadableStreamPushSource final : public ReadableStream<T> {
 public:
  ReadableStreamPushSource()
      : ReadableStream<T>(
            std::make_unique<CallbackUnderlyingSource<T>>(&controller_)) {}

  void Write(T chunk) override { controller_->Write(std::move(chunk)); }

 private:
  ReadableStreamController<T>* controller_;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_READABLE_STREAM_H_
