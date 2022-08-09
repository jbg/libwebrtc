/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/numerics/samples_stats_counter.h"

#include <algorithm>
#include <cmath>
#include <sstream>  // no-presubmit-check TODO(webrtc:8982)
#include <string>

#include "absl/algorithm/container.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

SamplesStatsCounter::SamplesStatsCounter() = default;
SamplesStatsCounter::~SamplesStatsCounter() = default;
SamplesStatsCounter::SamplesStatsCounter(const SamplesStatsCounter&) = default;
SamplesStatsCounter& SamplesStatsCounter::operator=(
    const SamplesStatsCounter&) = default;
SamplesStatsCounter::SamplesStatsCounter(SamplesStatsCounter&&) = default;
SamplesStatsCounter& SamplesStatsCounter::operator=(SamplesStatsCounter&&) =
    default;

void SamplesStatsCounter::AddSample(double value) {
  AddSample(StatsSample{value, Timestamp::Micros(rtc::TimeMicros())});
}

void SamplesStatsCounter::AddSample(StatsSample sample) {
  stats_.AddSample(sample.value);
  samples_.push_back(sample);
  sorted_ = false;
}

void SamplesStatsCounter::AddSamples(const SamplesStatsCounter& other) {
  stats_.MergeStatistics(other.stats_);
  samples_.insert(samples_.end(), other.samples_.begin(), other.samples_.end());
  sorted_ = false;
}

double SamplesStatsCounter::GetPercentile(double percentile) {
  RTC_DCHECK(!IsEmpty());
  RTC_CHECK_GE(percentile, 0);
  RTC_CHECK_LE(percentile, 1);
  if (!sorted_) {
    absl::c_sort(samples_, [](const StatsSample& a, const StatsSample& b) {
      return a.value < b.value;
    });
    sorted_ = true;
  }
  const double raw_rank = percentile * (samples_.size() - 1);
  double int_part;
  double fract_part = std::modf(raw_rank, &int_part);
  size_t rank = static_cast<size_t>(int_part);
  if (fract_part >= 1.0) {
    // It can happen due to floating point calculation error.
    rank++;
    fract_part -= 1.0;
  }

  RTC_DCHECK_GE(rank, 0);
  RTC_DCHECK_LT(rank, samples_.size());
  RTC_DCHECK_GE(fract_part, 0);
  RTC_DCHECK_LT(fract_part, 1);
  RTC_DCHECK(rank + fract_part == raw_rank);

  const double low = samples_[rank].value;
  const double high = samples_[std::min(rank + 1, samples_.size() - 1)].value;
  return low + fract_part * (high - low);
}

std::string SamplesStatsCounter::ToJson(absl::string_view graph_name,
                                        absl::string_view trace_name,
                                        absl::string_view units) const {
  std::ostringstream value_stream;  // no-presubmit-check TODO(webrtc:8982)
  value_stream.precision(8);
  value_stream << R"({"graph_name":")" << graph_name << R"(",)";
  value_stream << R"("trace_name":")" << trace_name << R"(",)";
  value_stream << R"("units":")" << units << R"(",)";
  if (!IsEmpty()) {
    value_stream << R"("mean":)" << GetAverage() << ',';
    value_stream << R"("std":)" << GetStandardDeviation() << ',';
  }
  value_stream << R"("samples":[)";
  const char* sep = "";
  for (const auto& sample : GetTimedSamples()) {
    value_stream << sep << R"({"time":)" << sample.time.us() << ','
                 << R"("value":)" << sample.value << '}';
    sep = ",";
  }
  value_stream << "]}";
  return value_stream.str();
}

SamplesStatsCounter operator*(const SamplesStatsCounter& counter,
                              double value) {
  SamplesStatsCounter out;
  for (const auto& sample : counter.GetTimedSamples()) {
    out.AddSample(
        SamplesStatsCounter::StatsSample{sample.value * value, sample.time});
  }
  return out;
}

SamplesStatsCounter operator/(const SamplesStatsCounter& counter,
                              double value) {
  SamplesStatsCounter out;
  for (const auto& sample : counter.GetTimedSamples()) {
    out.AddSample(
        SamplesStatsCounter::StatsSample{sample.value / value, sample.time});
  }
  return out;
}

}  // namespace webrtc
