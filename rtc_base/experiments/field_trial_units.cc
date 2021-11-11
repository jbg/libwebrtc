/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_units.h"

#include <stdio.h>

#include <limits>
#include <string>

#include "absl/strings/charconv.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace webrtc {
namespace {

struct ValueWithUnit {
  double value;
  absl::string_view unit;
};

absl::optional<ValueWithUnit> ParseValueWithUnit(absl::string_view str) {
  if (str == "inf") {
    return ValueWithUnit{std::numeric_limits<double>::infinity(), ""};
  }
  if (str == "-inf") {
    return ValueWithUnit{-std::numeric_limits<double>::infinity(), ""};
  }
  ValueWithUnit result;
  const char* const str_end = str.data() + str.size();
  absl::from_chars_result r =
      absl::from_chars(str.data(), str_end, result.value);
  if (r.ec != std::errc()) {
    return absl::nullopt;
  }
  result.unit = absl::string_view(r.ptr, str_end - r.ptr);
  return result;
}
}  // namespace

template <>
absl::optional<DataRate> ParseTypedParameter<DataRate>(absl::string_view str) {
  absl::optional<ValueWithUnit> result = ParseValueWithUnit(str);
  if (result) {
    if (result->unit.empty() || result->unit == "kbps") {
      return DataRate::KilobitsPerSec(result->value);
    } else if (result->unit == "bps") {
      return DataRate::BitsPerSec(result->value);
    }
  }
  return absl::nullopt;
}

template <>
absl::optional<DataSize> ParseTypedParameter<DataSize>(absl::string_view str) {
  absl::optional<ValueWithUnit> result = ParseValueWithUnit(str);
  if (result) {
    if (result->unit.empty() || result->unit == "bytes")
      return DataSize::Bytes(result->value);
  }
  return absl::nullopt;
}

template <>
absl::optional<TimeDelta> ParseTypedParameter<TimeDelta>(
    absl::string_view str) {
  absl::optional<ValueWithUnit> result = ParseValueWithUnit(str);
  if (result) {
    if (result->unit == "s" || result->unit == "seconds") {
      return TimeDelta::Seconds(result->value);
    } else if (result->unit == "us") {
      return TimeDelta::Micros(result->value);
    } else if (result->unit.empty() || result->unit == "ms") {
      return TimeDelta::Millis(result->value);
    }
  }
  return absl::nullopt;
}

template <>
absl::optional<absl::optional<DataRate>>
ParseTypedParameter<absl::optional<DataRate>>(absl::string_view str) {
  return ParseOptionalParameter<DataRate>(str);
}
template <>
absl::optional<absl::optional<DataSize>>
ParseTypedParameter<absl::optional<DataSize>>(absl::string_view str) {
  return ParseOptionalParameter<DataSize>(str);
}
template <>
absl::optional<absl::optional<TimeDelta>>
ParseTypedParameter<absl::optional<TimeDelta>>(absl::string_view str) {
  return ParseOptionalParameter<TimeDelta>(str);
}

template class FieldTrialParameter<DataRate>;
template class FieldTrialParameter<DataSize>;
template class FieldTrialParameter<TimeDelta>;

template class FieldTrialConstrained<DataRate>;
template class FieldTrialConstrained<DataSize>;
template class FieldTrialConstrained<TimeDelta>;

template class FieldTrialOptional<DataRate>;
template class FieldTrialOptional<DataSize>;
template class FieldTrialOptional<TimeDelta>;
}  // namespace webrtc
