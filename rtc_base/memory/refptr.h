/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_MEMORY_REFPTR_H_
#define RTC_BASE_MEMORY_REFPTR_H_

#include <utility>

#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counter.h"

namespace webrtc {
namespace webrtc_impl {
template <typename T>
class RefCountWrapper : public T, public rtc::RefCountInterface {
 public:
  explicit RefCountWrapper(T&& obj) : T(std::move(obj)) {}
  void AddRef() const override { ref_count_.IncRef(); }
  rtc::RefCountReleaseStatus Release() const override {
    const auto status = ref_count_.DecRef();
    if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
    return status;
  }
  virtual bool HasOneRef() const { return ref_count_.HasOneRef(); }

 protected:
  ~RefCountWrapper() override {}
  mutable webrtc_impl::RefCounter ref_count_{0};
};
}  // namespace webrtc_impl
template <typename T>
class RefPtr : public rtc::scoped_refptr<webrtc_impl::RefCountWrapper<T>> {
 public:
  RefPtr() = default;
  explicit RefPtr(T&& obj)
      : rtc::scoped_refptr<webrtc_impl::RefCountWrapper<T>>(
            new webrtc_impl::RefCountWrapper<T>(std::move(obj))) {}
};

template <typename T, typename... Args>
RefPtr<T> MakeRefPtr(Args... args) {
  return RefPtr<T>(T(args...));
}
template <typename T>
RefPtr<T> WrapRefPtr(T&& obj) {
  return RefPtr<T>(std::move(obj));
}

}  // namespace webrtc
#endif  // RTC_BASE_MEMORY_REFPTR_H_
