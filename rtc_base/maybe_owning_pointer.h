/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_MAYBE_OWNING_POINTER_H_
#define RTC_BASE_MAYBE_OWNING_POINTER_H_

#include <memory>

namespace webrtc {

template <typename Interface, typename Default>
class MaybeOwningPointer {
 public:
  explicit MaybeOwningPointer(Interface* pointer)
      : owned_instance_(pointer ? nullptr : std::make_unique<Default>()),
        pointer_(pointer ? pointer : owned_instance_.get()) {}

  Interface* get() { return pointer_; }
  Interface& operator*() { return *pointer_; }

 private:
  std::unique_ptr<Interface> owned_instance_;
  Interface* const pointer_;
};

}  // namespace webrtc

#endif  // RTC_BASE_MAYBE_OWNING_POINTER_H_
