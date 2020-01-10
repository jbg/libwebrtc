/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/optional_conversions.h"

#include <limits>

#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

int OptionalSizeTToInt(const absl::optional<size_t>& optional) {
  return optional.has_value()
             ? rtc::dchecked_cast<int, size_t>(optional.value())
             : std::numeric_limits<int>::max();
}

absl::optional<int> OptionalSizeTToOptionalInt(
    const absl::optional<size_t>& optional) {
  return optional.has_value()
             ? absl::optional<int>(
                   rtc::dchecked_cast<int, size_t>(optional.value()))
             : absl::nullopt;
}

int OptionalDoubleToInt(const absl::optional<double>& optional) {
  return optional.has_value() ? static_cast<int>(optional.value())
                              : std::numeric_limits<int>::max();
}

}  // namespace webrtc
