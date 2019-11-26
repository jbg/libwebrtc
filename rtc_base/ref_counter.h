/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REF_COUNTER_H_
#define RTC_BASE_REF_COUNTER_H_

#include <atomic>

#include "rtc_base/ref_count.h"

namespace webrtc {
namespace webrtc_impl {

class RefCounter {
 public:
  explicit RefCounter(int ref_count) : ref_count_(ref_count) {}
  RefCounter() = delete;

  void IncRef() {
    // No barrier: when this is the first reference, current thread should be
    // the only thread that has access to the object protected by the reference
    // counting and thus doesn't need extra synchronization.
    // When this is not the first reference, this increase doesn't participate
    // in synchronizations that makes the ref_count_ one or zero.
    ref_count_.fetch_add(1, std::memory_order_relaxed);
  }

  // Returns kDroppedLastRef if this call dropped the last reference; the caller
  // should therefore free the resource protected by the reference counter.
  // Otherwise, returns kOtherRefsRemained (note that in case of multithreading,
  // some other caller may have dropped the last reference by the time this call
  // returns; all we know is that we didn't do it).
  rtc::RefCountReleaseStatus DecRef() {
    // Insert acquire-release barrier to ensure that state written before the
    // reference count became zero will be visible to a thread that has just
    // made the count zero.
    int ref_count_after_subtract =
        ref_count_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    return ref_count_after_subtract == 0
               ? rtc::RefCountReleaseStatus::kDroppedLastRef
               : rtc::RefCountReleaseStatus::kOtherRefsRemained;
  }

  // Return whether the reference count is one. If the reference count is used
  // in the conventional way, a reference count of 1 implies that the current
  // thread owns the reference and no other thread shares it. This call performs
  // the test for a reference count of one, and performs the memory barrier
  // needed for the owning thread to act on the resource protected by the
  // reference counter, knowing that it has exclusive access.
  bool HasOneRef() const {
    // Insert acquire-release barrier to ensure that state written before the
    // reference count became one (i.e. std::memory_order_release in DecRef)
    // will be visible to a thread that checks the count is one.
    return ref_count_.load(std::memory_order_acquire) == 1;
  }

 private:
  std::atomic<int> ref_count_;
};

}  // namespace webrtc_impl
}  // namespace webrtc

#endif  // RTC_BASE_REF_COUNTER_H_
