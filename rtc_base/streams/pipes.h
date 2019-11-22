/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_PIPES_H_
#define RTC_BASE_STREAMS_PIPES_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/types/variant.h"
#include "rtc_base/checks.h"

namespace webrtc {

template <typename T>
class ReadableStream;
class ReadableStreamBase;
template <typename T>
class WritableStream;
class WritableStreamBase;
template <typename I, typename O>
class TransformStream;
class TransformStreamBase;

class StreamConnection final {
 public:
  explicit StreamConnection(WritableStreamBase*);
  explicit StreamConnection(std::unique_ptr<WritableStreamBase>);
  explicit StreamConnection(std::unique_ptr<TransformStreamBase>);
  ~StreamConnection();

  StreamConnection(const StreamConnection&) = delete;
  StreamConnection(StreamConnection&&);
  StreamConnection& operator=(const StreamConnection&) = delete;
  StreamConnection& operator=(StreamConnection&&);

  operator bool() const { return static_cast<bool>(writable()); }

  WritableStreamBase* writable() const;

  void SetOwnedReadable(std::unique_ptr<ReadableStreamBase> readable) {
    readable_owned_ = std::move(readable);
  }

  void Close();

 private:
  void Disconnect();

  absl::variant<WritableStreamBase*,
                std::unique_ptr<WritableStreamBase>,
                std::unique_ptr<TransformStreamBase>>
      writable_;
  std::unique_ptr<ReadableStreamBase> readable_owned_;
};

struct PipeToHandle final {
 public:
  PipeToHandle() = default;
  explicit PipeToHandle(StreamConnection connection);
  ~PipeToHandle();

  PipeToHandle(const PipeToHandle&) = delete;
  PipeToHandle(PipeToHandle&&) = default;
  PipeToHandle& operator=(const PipeToHandle&) = delete;
  PipeToHandle& operator=(PipeToHandle&&) = default;

  operator bool() const { return IsConnected(); }

  bool IsConnected() const { return !connections_.empty(); }

  void Close();

 private:
  template <typename O>
  friend class PipeThroughHandle;

  std::vector<StreamConnection> connections_;
};

template <typename O>
class PipeThroughHandle final {
 public:
  PipeThroughHandle() = default;

  PipeThroughHandle(PipeToHandle input_pipe, ReadableStream<O>* readable)
      : input_pipe_(std::move(input_pipe)), readable_(readable) {
    RTC_DCHECK(input_pipe_);
    RTC_DCHECK(readable_);
  }

  PipeToHandle input_pipe() && { return std::move(input_pipe_); }

  ReadableStream<O>* readable() { return readable_; }

  PipeToHandle PipeTo(WritableStream<O>* destination) && {
    RTC_DCHECK(destination);
    destination->Connect(readable_);
    input_pipe_.connections_.push_back(StreamConnection(destination));
    return std::move(input_pipe_);
  }

  template <typename T>
  PipeThroughHandle<T> PipeThrough(TransformStream<O, T>* transform) && {
    RTC_DCHECK(transform);
    transform->writable()->Connect(readable_);
    input_pipe_.connections_.push_back(StreamConnection(transform->writable()));
    return PipeThroughHandle<T>(std::move(input_pipe_), transform->readable());
  }

  template <typename T>
  PipeThroughHandle<T> PipeThrough(
      std::unique_ptr<TransformStream<O, T>> transform) && {
    RTC_DCHECK(transform);
    transform->writable()->Connect(readable_);
    ReadableStream<T>* readable = transform->readable();
    input_pipe_.connections_.push_back(StreamConnection(std::move(transform)));
    return PipeThroughHandle<T>(std::move(input_pipe_), readable);
  }

 private:
  PipeToHandle input_pipe_;
  ReadableStream<O>* readable_;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_PIPES_H_
