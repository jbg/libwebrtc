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

namespace webrtc {

template <typename T>
class ReadableStream;

template <typename T>
class WritableStreamController {
 public:
  // GetCompletionCallback.
};

template <typename T>
class UnderlyingSink {
 public:
  virtual ~UnderlyingSink() = default;

  virtual void Start(WritableStreamController<T>*) = 0;

  virtual void Write(T chunk, WritableStreamController<T>*) = 0;
};

class WritableStreamBase {
 public:
  virtual ~WritableStreamBase() = default;

 protected:
};

template <typename T>
class WritableStream final : public WritableStreamBase,
                             private WritableStreamController<T> {
 public:
  WritableStream(std::unique_ptr<UnderlyingSink<T>> underlying_sink)
      : underlying_sink_(std::move(underlying_sink)) {
    RTC_DCHECK(underlying_sink_);
    underlying_sink_->Start(this);
  }

 private:
  friend class ReadableStream<T>;
  bool IsReady() {
    // TODO: implement backpressure.
    return true;
  }

  void Enqueue(T chunk) { underlying_sink_->Write(std::move(chunk), this); }

  std::unique_ptr<UnderlyingSink<T>> underlying_sink_;
  ReadableStream<T>* origin_ = nullptr;
};

template <typename T>
class WritableStreamWriter {
  bool IsReady();
  bool IsReady(std::function<void()> ready_cb);

  void Write(T chunk);
};

template <typename T>
class CallbackUnderlyingSink final : public UnderlyingSink<T> {
 public:
  CallbackUnderlyingSink(std::function<void(T)> callback)
      : callback_(std::move(callback)) {
    RTC_DCHECK(callback_);
  }

  // UnderlyingSink implementation.
  void Start(WritableStreamController<T>*) override {}
  void Write(T chunk, WritableStreamController<T>*) override {
    callback_(std::move(chunk));
  }

 private:
  std::function<void(T)> callback_;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_WRITABLE_STREAM_H_
