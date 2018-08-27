/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CANCELABLE_TASK_HANDLER_H_
#define RTC_BASE_CANCELABLE_TASK_HANDLER_H_

#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/task_queue.h"

namespace rtc {

class BaseCancelableTask;

class CancelableTaskHandler {
 public:
  // This class is copyable and cheaply movable.
  CancelableTaskHandler();
  CancelableTaskHandler(const CancelableTaskHandler&);
  CancelableTaskHandler(CancelableTaskHandler&&);
  CancelableTaskHandler& operator=(const CancelableTaskHandler&);
  CancelableTaskHandler& operator=(CancelableTaskHandler&&);
  // Deleteing the handler doesn't Cancel the task.
  ~CancelableTaskHandler();

  // Prevents scheduling new runnings of the task.
  // Doesn't wait if task() is already running.
  void Cancel();

 private:
  friend class BaseCancelableTask;
  class CancelationToken;
  explicit CancelableTaskHandler(
      rtc::scoped_refptr<CancelationToken> cancelation_token);

  rtc::scoped_refptr<CancelationToken> cancelation_token_;
};

class BaseCancelableTask : public QueuedTask {
 public:
  ~BaseCancelableTask() override;

  CancelableTaskHandler CancelationHandler() const;

 protected:
  BaseCancelableTask();

  bool Canceled() const;

 private:
  rtc::scoped_refptr<CancelableTaskHandler::CancelationToken>
      cancelation_token_;
};

}  // namespace rtc

#endif  // RTC_BASE_CANCELABLE_TASK_HANDLER_H_
