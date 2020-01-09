/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_OPTIONAL_CONVERSIONS_H_
#define RTC_BASE_NUMERICS_OPTIONAL_CONVERSIONS_H_

#include "absl/types/optional.h"

namespace webrtc {

// The missing value is represented by std::numeric_limits<int>::max().
int OptionalSizeTToInt(const absl::optional<size_t>& optional);

absl::optional<int> OptionalSizeTToOptionalInt(
    const absl::optional<size_t>& optional);

// The missing value is represented by std::numeric_limits<int>::max().
int OptionalDoubleToInt(const absl::optional<double>& optional);

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_OPTIONAL_CONVERSIONS_H_
