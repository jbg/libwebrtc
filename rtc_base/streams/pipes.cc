/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/streams/pipes.h"

#include <memory>
#include <utility>

#include "rtc_base/streams/readable_stream.h"
#include "rtc_base/streams/transform_stream.h"
#include "rtc_base/streams/writable_stream.h"

namespace webrtc {
namespace {

struct WritableStreamVisitor {
 public:
  WritableStreamBase* operator()(WritableStreamBase* writable) {
    return writable;
  }

  WritableStreamBase* operator()(
      const std::unique_ptr<WritableStreamBase>& writable) {
    return writable.get();
  }

  WritableStreamBase* operator()(
      const std::unique_ptr<TransformStreamBase>& transform) {
    return transform->writable();
  }
};

}  // namespace

StreamConnection::StreamConnection(WritableStreamBase* writable)
    : writable_(writable) {
  RTC_DCHECK(absl::get<WritableStreamBase*>(writable_));
}

StreamConnection::StreamConnection(std::unique_ptr<WritableStreamBase> writable)
    : writable_(std::move(writable)) {
  RTC_DCHECK(absl::get<std::unique_ptr<WritableStreamBase>>(writable_));
}

StreamConnection::StreamConnection(
    std::unique_ptr<TransformStreamBase> transform)
    : writable_(std::move(transform)) {
  RTC_DCHECK(absl::get<std::unique_ptr<TransformStreamBase>>(writable_));
}

StreamConnection::~StreamConnection() {
  Disconnect();
}

StreamConnection::StreamConnection(StreamConnection&& o) : writable_(nullptr) {
  std::swap(writable_, o.writable_);
  std::swap(readable_owned_, o.readable_owned_);
}

StreamConnection& StreamConnection::operator=(StreamConnection&& o) {
  Disconnect();
  std::swap(writable_, o.writable_);
  std::swap(readable_owned_, o.readable_owned_);
  return *this;
}

WritableStreamBase* StreamConnection::writable() const {
  return absl::visit(WritableStreamVisitor(), writable_);
}

void StreamConnection::Close() {
  Disconnect();
  writable_ = nullptr;
  readable_owned_.reset();
}

void StreamConnection::Disconnect() {
  WritableStreamBase* stream = writable();
  if (stream) {
    stream->Disconnect();
  }
}

PipeToHandle::PipeToHandle(StreamConnection connection) {
  connections_.push_back(std::move(connection));
}

PipeToHandle::~PipeToHandle() {
  Close();
}

void PipeToHandle::Close() {
  for (auto it = connections_.rbegin(); it != connections_.rend(); ++it) {
    it->Close();
  }
  connections_.clear();
}

}  // namespace webrtc
