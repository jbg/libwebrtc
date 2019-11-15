/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/streams/readable_stream.h"

namespace webrtc {

void ReadableStreamBase::OnStartAsync() {
  switch (state_) {
    case State::kStarting:
      state_ = State::kStartPending;
      break;
    case State::kPulling:
      state_ = State::kPullPending;
      break;
    default:
      RTC_NOTREACHED();
  }
}

void ReadableStreamBase::OnCompleteAsync() {
  switch (state_) {
    case State::kStartPending:
      state_ = State::kReady;
      PokeDestination();
      break;
    case State::kPullPending:
      state_ = State::kIdle;
      break;
    case State::kPullPendingProduced:
      state_ = State::kReady;
      PokeDestination();
      break;
    default:
      RTC_NOTREACHED();
  }
}

void ReadableStreamBase::OnWrite() {
  switch (state_) {
    case State::kPulling:
      state_ = State::kPullingProduced;
      break;
    case State::kPullPending:
      state_ = State::kPullPendingProduced;
      break;
    default:
      break;
  }
}

void ReadableStreamBase::OnBlocked() const {
  if (state_ == State::kIdle) {
    const_cast<ReadableStreamBase*>(this)->state_ = State::kReady;
    // No need to |PokeDestination()| since this is only called if the
    // destination is not ready.
  }
}

bool ReadableStreamBase::IsReady() const {
  return state_ == State::kReady;
}

void ReadableStreamBase::Start() {
  if (state_ != State::kInit) {
    return;
  }
  state_ = State::kStarting;
  StartUnderlyingSource();
  if (state_ != State::kStarting) {
    return;
  }
  state_ = State::kReady;
}

void ReadableStreamBase::Pull() {
  if (!IsReady()) {
    return;
  }
  state_ = State::kPulling;
  PullUnderlyingSource();
  switch (state_) {
    case State::kPulling:
      state_ = State::kIdle;
      return;
    case State::kPullingProduced:
      state_ = State::kReady;
      break;
    default:
      return;
  }
}

}  // namespace webrtc
