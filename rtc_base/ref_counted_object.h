/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REF_COUNTED_OBJECT_H_
#define RTC_BASE_REF_COUNTED_OBJECT_H_

#include <type_traits>
#include <utility>

#include "rtc_base/constructor_magic.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counter.h"

namespace rtc {

template <class T>
class RefCountedObject final : public T {
 public:
  template <typename... Args>
  explicit RefCountedObject(Args&&... args) : T(std::forward<Args>(args)...) {}

  void AddRef() const { ref_count_.IncRef(); }
  void Release() const {
    if (ref_count_.DecRef()) {
      delete this;
    }
  }

  // Return whether the reference count is one. If the reference count is used
  // in the conventional way, a reference count of 1 implies that the current
  // thread owns the reference and no other thread shares it. This call
  // performs the test for a reference count of one, and performs the memory
  // barrier needed for the owning thread to act on the object, knowing that it
  // has exclusive access to the object.
  bool HasOneRef() const { return ref_count_.HasOneRef(); }

 protected:
  ~RefCountedObject() {}

  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};
};

}  // namespace rtc

#endif  // RTC_BASE_REF_COUNTED_OBJECT_H_
