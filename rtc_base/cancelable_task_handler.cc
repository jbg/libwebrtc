/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cancelable_task_handler.h"

#include <atomic>
#include <utility>

#include "rtc_base/refcounter.h"

namespace rtc {

class CancelableTaskHandler::CancelationToken {
 public:
  CancelationToken() : canceled_(false), ref_count_(0) {}
  CancelationToken(const CancelationToken&) = delete;
  CancelationToken& operator=(const CancelationToken&) = delete;

  void Cancel() { canceled_.store(true, std::memory_order_release); }

  bool Canceled() { return canceled_.load(std::memory_order_acquire); }

  void AddRef() { ref_count_.IncRef(); }

  void Release() {
    if (ref_count_.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef)
      delete this;
  }

 private:
  ~CancelationToken() = default;

  std::atomic<bool> canceled_;
  webrtc::webrtc_impl::RefCounter ref_count_;
};

CancelableTaskHandler::CancelableTaskHandler() = default;
CancelableTaskHandler::CancelableTaskHandler(const CancelableTaskHandler&) =
    default;
CancelableTaskHandler::CancelableTaskHandler(CancelableTaskHandler&&) = default;
CancelableTaskHandler& CancelableTaskHandler::operator=(
    const CancelableTaskHandler&) = default;
CancelableTaskHandler& CancelableTaskHandler::operator=(
    CancelableTaskHandler&&) = default;
CancelableTaskHandler::~CancelableTaskHandler() = default;

void CancelableTaskHandler::Cancel() {
  if (cancelation_token_.get() != nullptr)
    cancelation_token_->Cancel();
}

CancelableTaskHandler::CancelableTaskHandler(
    rtc::scoped_refptr<CancelationToken> cancelation_token)
    : cancelation_token_(std::move(cancelation_token)) {}

BaseCancelableTask::~BaseCancelableTask() = default;

CancelableTaskHandler BaseCancelableTask::CancelationHandler() const {
  return CancelableTaskHandler(cancelation_token_);
}

BaseCancelableTask::BaseCancelableTask()
    : cancelation_token_(new CancelableTaskHandler::CancelationToken) {}

bool BaseCancelableTask::Canceled() const {
  return cancelation_token_->Canceled();
}

}  // namespace rtc
