/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/histogram_percentile_counter.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/checks.h"

namespace rtc {
HistogramPercentileCounter::HistogramPercentileCounter(
    size_t long_tail_boundary)
    : histogram_low_(long_tail_boundary),
      long_tail_boundary_(long_tail_boundary),
      total_elements_(0),
      total_elements_low_(0) {}

HistogramPercentileCounter::~HistogramPercentileCounter() {}

void HistogramPercentileCounter::Add(const HistogramPercentileCounter& other) {
  uint32_t value;
  for (value = 0; value < other.long_tail_boundary_; ++value) {
    Add(value, other.histogram_low_[value]);
  }
  for (auto it = other.histogram_high_.begin();
       it != other.histogram_high_.end(); ++it) {
    Add(it->first, it->second);
  }
}

void HistogramPercentileCounter::Add(uint32_t value, size_t count) {
  if (value < long_tail_boundary_) {
    histogram_low_[value] += count;
    total_elements_low_ += count;
  } else {
    histogram_high_[value] += count;
  }
  total_elements_ += count;
}

void HistogramPercentileCounter::Add(uint32_t value) {
  Add(value, 1);
}

rtc::Optional<uint32_t> HistogramPercentileCounter::GetPercentile(
    float fraction) {
  RTC_CHECK_LE(fraction, 1);
  RTC_CHECK_GE(fraction, 0);
  if (total_elements_ == 0)
    return rtc::Optional<uint32_t>();
  int elements_to_skip =
      static_cast<int>(std::ceil(total_elements_ * fraction)) - 1;
  if (elements_to_skip < 0)
    elements_to_skip = 0;
  if (elements_to_skip >= static_cast<int>(total_elements_))
    elements_to_skip = total_elements_ - 1;
  if (elements_to_skip < static_cast<int>(total_elements_low_)) {
    for (uint32_t value = 0; value < long_tail_boundary_; ++value) {
      elements_to_skip -= histogram_low_[value];
      if (elements_to_skip < 0)
        return rtc::Optional<uint32_t>(value);
    }
  } else {
    elements_to_skip -= total_elements_low_;
    for (auto it = histogram_high_.begin(); it != histogram_high_.end(); ++it) {
      elements_to_skip -= it->second;
      if (elements_to_skip < 0)
        return rtc::Optional<uint32_t>(it->first);
    }
  }
  RTC_NOTREACHED();
  return rtc::Optional<uint32_t>();
}

}  // namespace rtc
