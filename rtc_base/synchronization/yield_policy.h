/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYNCHRONIZATION_YIELD_POLICY_H_
#define RTC_BASE_SYNCHRONIZATION_YIELD_POLICY_H_

#include <memory>

namespace rtc {
class EventInterface {
 public:
  virtual ~EventInterface() = default;
  virtual void Reset() = 0;
  virtual void Set() = 0;
  virtual bool Wait(int give_up_after_ms, int warn_after_ms) = 0;
};

class YieldInterface {
 public:
  virtual ~YieldInterface() = default;
  virtual void YieldExecution() = 0;
  virtual std::unique_ptr<EventInterface> CreateEvent(
      bool manual_reset,
      bool initially_signaled) = 0;
};

// Sets the current thread-local yield policy while it's in scope and reverts
// to the previous policy when it leaves the scope.
class ScopedYieldPolicy final {
 public:
  explicit ScopedYieldPolicy(YieldInterface* policy);
  ScopedYieldPolicy(const ScopedYieldPolicy&) = delete;
  ScopedYieldPolicy& operator=(const ScopedYieldPolicy&) = delete;
  ~ScopedYieldPolicy();
  // Will yield as specified by the currently active thread-local yield policy
  // (which by default is a no-op).
  static void YieldExecution();
  static bool Active();
  static std::unique_ptr<EventInterface> CreateEvent(bool manual_reset,
                                                     bool initially_signaled);

 private:
  YieldInterface* const previous_;
};

}  // namespace rtc

#endif  // RTC_BASE_SYNCHRONIZATION_YIELD_POLICY_H_
