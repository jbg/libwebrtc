/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/streams/writable_stream.h"

namespace webrtc {

void WritableStreamBase::OnStartAsync() {
  switch (state_) {
    case State::kStarting:
      state_ = State::kStartPending;
      break;
    case State::kWriting:
      state_ = State::kWritePending;
      break;
    case State::kClosing:
      state_ = State::kClosePending;
      break;
    default:
      RTC_NOTREACHED();
  }
}

void WritableStreamBase::OnCompleteAsync() {
  switch (state_) {
    case State::kStartPending:
    case State::kWritePending:
      state_ = State::kReady;
      PollSource();
      break;
    case State::kStartPendingCloseRequest:
    case State::kWritePendingCloseRequest:
      state_ = State::kReady;
      Close();
      break;
    case State::kClosePending:
      state_ = State::kClosed;
      break;
    default:
      RTC_NOTREACHED();
  }
}

bool WritableStreamBase::IsReady() const {
  return state_ == State::kReady;
}

void WritableStreamBase::Start() {
  if (state_ == State::kInit) {
    state_ = State::kStarting;
    StartUnderlyingSink();
    if (state_ != State::kStarting) {
      return;
    }
    state_ = State::kReady;
  }
  PollSource();
}

void WritableStreamBase::PollSource() {
  while (IsReady() && origin()->IsReady()) {
    origin()->Pull();
  }
}

void WritableStreamBase::Close() {
  RTC_DCHECK(!IsReentrant());

  switch (state_) {
    case State::kStartPending:
      state_ = State::kStartPendingCloseRequest;
      return;
    case State::kWritePending:
      state_ = State::kWritePendingCloseRequest;
      return;
    case State::kClosePending:
    case State::kClosed:
      return;
    default:
      break;
  }

  state_ = State::kClosing;
  CloseUnderlyingSink();
  if (state_ != State::kClosing) {
    return;
  }
  state_ = State::kClosed;
}

}  // namespace webrtc
