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

#include "rtc_base/streams/writable_stream.h"

namespace webrtc {

template <typename T>
class ReadableStreamController {
 public:
  virtual ~ReadableStreamController() = default;

  virtual bool IsReady() const = 0;

  virtual void Enqueue(T chunk) = 0;

  virtual std::function<void()> GetCompletionCallback() = 0;
};

template <typename T>
class UnderlyingSource {
 public:
  virtual ~UnderlyingSource() = default;

  virtual void Start(ReadableStreamController<T>*) = 0;

  virtual void Pull(ReadableStreamController<T>*) = 0;
};

template <typename T>
class CallbackUnderlyingSource final : public UnderlyingSource<T> {
 public:
  CallbackUnderlyingSource(ReadableStreamController<T>** controller_out)
      : controller_out_(controller_out) {}

  // UnderlyingSource implementation.
  void Start(ReadableStreamController<T>* controller) override {
    *controller_out_ = controller;
  }
  void Pull(ReadableStreamController<T>*) override {}

 private:
  ReadableStreamController<T>** controller_out_;
};

class ReadableStreamBase {
 public:
  virtual ~ReadableStreamBase() = default;

 protected:
  friend struct PipeToHandle;

  virtual void Release() = 0;
};

struct PipeToHandle final {
 public:
  PipeToHandle() = default;
  explicit PipeToHandle(ReadableStreamBase* readable) : readable_(readable) {
    RTC_DCHECK(readable_);
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
  }

 private:
  ReadableStreamBase* readable_ = nullptr;
};

// struct PipeThroughHandle final {
//  private:
//   absl::variant<PipeThroughHandle,
//                 std::unique_ptr<TransformStreamBase>,
//                 TransformStreamBase*>
//       owned_;
// };

template <typename T>
class ReadableStream final : public ReadableStreamBase,
                             private ReadableStreamController<T> {
 public:
  ReadableStream(std::unique_ptr<UnderlyingSource<T>> source)
      : source_(std::move(source)) {
    RTC_DCHECK(source_);
    source_->Start(this);
  }

  PipeToHandle PipeTo(WritableStream<T>* destination) {
    RTC_DCHECK(destination);
    RTC_DCHECK(!destination_);
    RTC_DCHECK(!destination->origin_);
    destination_ = destination;
    destination->origin_ = this;
    return PipeToHandle{this};
  }

  bool IsLocked() { return static_cast<bool>(destination_); }

  // PipeThroughHandle PipeThrough(StreamTransform<T>* transform);

 private:
  // ReadableStreamBase implementation.
  void Release() override {
    RTC_DCHECK(destination_);
    destination_->origin_ = nullptr;
    destination_ = nullptr;
  }

  // ReadableStreamController implementation.
  bool IsReady() const override {
    return destination_ && destination_->IsReady();
  }
  void Enqueue(T chunk) override {
    if (IsReady()) {
      destination_->Enqueue(std::move(chunk));
    }
  }
  std::function<void()> GetCompletionCallback() override {
    // TODO
    RTC_DCHECK(false);
  }

  std::unique_ptr<UnderlyingSource<T>> source_;
  WritableStream<T>* destination_ = nullptr;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_READABLE_STREAM_H_
