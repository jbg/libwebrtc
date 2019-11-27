/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_REF_COUNTED_BASE_H_
#define API_REF_COUNTED_BASE_H_

#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counter.h"

namespace rtc {

class RefCountedBase {
 public:
  RefCountedBase() = default;
  RefCountedBase(const RefCountedBase&) = delete;
  RefCountedBase& operator=(const RefCountedBase&) = delete;

  void AddRef() const { ref_count_.IncRef(); }
  void Release() const {
    if (ref_count_.DecRef() == RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
  }

 protected:
  virtual ~RefCountedBase() = default;

 private:
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};
};

}  // namespace rtc

#endif  // API_REF_COUNTED_BASE_H_
