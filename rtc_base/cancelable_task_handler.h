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

// Allows to cancel a cancelable task. Non-empty handler can be acquired by
// calling CancelationHandler() on a cancelable task.
class CancelableTaskHandler {
 public:
  // This class is copyable and cheaply movable.
  CancelableTaskHandler();
  CancelableTaskHandler(const CancelableTaskHandler&);
  CancelableTaskHandler(CancelableTaskHandler&&);
  CancelableTaskHandler& operator=(const CancelableTaskHandler&);
  CancelableTaskHandler& operator=(CancelableTaskHandler&&);
  // Deleting the handler doesn't Cancel the task.
  ~CancelableTaskHandler();

  // Prevents the cancelable task to run.
  // Must be executed on the same task queue as the task itself.
  void Cancel() const;

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
