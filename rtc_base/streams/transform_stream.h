/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_TRANSFORM_STREAM_H_
#define RTC_BASE_STREAMS_TRANSFORM_STREAM_H_

#include <memory>

#include "rtc_base/streams/readable_stream.h"
#include "rtc_base/streams/writable_stream.h"

namespace webrtc {

class TransformStreamBase {
 public:
  virtual ~TransformStreamBase() = default;
};

template <typename T>
class TransformStream final : public TransformStreamBase {
 public:
  TransformStream(WritableStream<T>* writable, ReadableStream<T>* readable)
      : writable_(writable), readable_(readable) {
    RTC_DCHECK(writable_);
    RTC_DCHECK(readable_);
  }
  TransformStream(const TransformStream<T>&) = delete;
  TransformStream(TransformStream<T>&&) = delete;
  TransformStream<T>& operator=(const TransformStream<T>&) = delete;
  TransformStream<T>& operator=(TransformStream<T>&&) = delete;

  WritableStream<T>* writable() { return writable_; }
  ReadableStream<T>* readable() { return readable_; }

 private:
  WritableStream<T>* writable_;
  ReadableStream<T>* readable_;
};

template <typename T>
class IdentityTransformStream : public TransformStream<T> {};

template <typename T>
std::unique_ptr<ReadableStream<T>> JoinReadable2(ReadableStream<T>* in1,
                                                 ReadableStream<T>* in2) {}

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_TRANSFORM_STREAM_H_
