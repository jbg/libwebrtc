/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAMS_UNDERLYING_TRANSFORM_H_
#define RTC_BASE_STREAMS_UNDERLYING_TRANSFORM_H_

namespace webrtc {

template <typename O>
class TransformStreamController {
 public:
  virtual ~TransformStreamController() = default;

  virtual bool IsWritable() = 0;

  virtual void Write(O chunk) = 0;

  virtual void StartAsync() = 0;

  virtual void CompleteAsync() = 0;
};

template <typename I, typename O>
class UnderlyingTransform {
 public:
  virtual ~UnderlyingTransform() = default;

  virtual void Start(TransformStreamController<O>*) = 0;

  virtual void Transform(I chunk, TransformStreamController<O>*) = 0;

  virtual void Flush(TransformStreamController<O>*) = 0;

  virtual void Close(TransformStreamController<O>*) = 0;
};

}  // namespace webrtc

#endif  // RTC_BASE_STREAMS_UNDERLYING_TRANSFORM_H_
