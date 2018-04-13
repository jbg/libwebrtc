/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/units/data_rate.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
DataRate operator/(const DataSize& size, const TimeDelta& duration) {
  RTC_DCHECK(size.bytes() < std::numeric_limits<int64_t>::max() / 1000000)
      << "size is too large, size: " << size.bytes() << " is not less than "
      << std::numeric_limits<int64_t>::max() / 1000000;
  int64_t bytes_per_sec = size.bytes() * 1000000 / duration.us();
  return DataRate::bytes_per_second(bytes_per_sec);
}

TimeDelta operator/(const DataSize& size, const DataRate& rate) {
  RTC_DCHECK(size.bytes() < std::numeric_limits<int64_t>::max() / 1000000)
      << "size is too large, size: " << size.bytes() << " is not less than "
      << std::numeric_limits<int64_t>::max() / 1000000;
  int64_t microseconds = size.bytes() * 1000000 / rate.bytes_per_second();
  return TimeDelta::us(microseconds);
}

DataSize operator*(const DataRate& rate, const TimeDelta& duration) {
  int64_t micro_bytes = rate.bytes_per_second() * duration.us();
  return DataSize::bytes((micro_bytes + 500000) / 1000000);
}

DataSize operator*(const TimeDelta& duration, const DataRate& rate) {
  return rate * duration;
}

std::string ToString(const DataRate& value) {
  char buf[64];
  rtc::SimpleStringBuilder sb(buf);
  if (value.IsInfinite()) {
    sb << "inf bps";
  } else if (!value.IsInitialized()) {
    sb << "? bps";
  } else {
    sb << value.bps() << " bps";
  }
  return sb.str();
}
}  // namespace webrtc
