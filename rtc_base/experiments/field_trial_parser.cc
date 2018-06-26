/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_parser.h"

#include <string.h>

#include <algorithm>
#include <type_traits>
#include <utility>

#include "rtc_base/logging.h"

namespace webrtc {
namespace {

int FindOrEnd(const string_view str, size_t start, char delimiter) {
  for (size_t pos = start; pos < str.length(); ++pos) {
    if (str[pos] == delimiter)
      return static_cast<int>(pos);
  }
  return static_cast<int>(str.length());
}
}  // namespace
string_view::string_view(const char* c_str)
    : string_view(c_str, strlen(c_str)) {}
string_view::string_view(const char* data, size_t length)
    : data_(data), size_(length) {}
string_view::string_view(const std::string& string)
    : string_view(string.c_str(), string.size()) {}

bool string_view::operator==(string_view other) const {
  return size_ == other.size_ && memcmp(data_, other.data_, size_) == 0;
}
string_view string_view::substr(int pos, int len) const {
  return string_view(data_ + pos, len);
}

FieldTrialParameterInterface::FieldTrialParameterInterface(
    const string_view key)
    : key_(key) {}
FieldTrialParameterInterface::~FieldTrialParameterInterface() = default;
const string_view FieldTrialParameterInterface::Key() const {
  return key_;
}

void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    const string_view trial_string) {
  size_t i = 0;
  while (i < trial_string.length()) {
    int val_end = FindOrEnd(trial_string, i, ',');
    int colon_pos = FindOrEnd(trial_string, i, ':');
    int key_end = std::min(val_end, colon_pos);
    int val_begin = key_end + 1;
    const string_view key = trial_string.substr(i, key_end - i);
    absl::optional<string_view> opt_value;
    if (val_end >= val_begin)
      opt_value = trial_string.substr(val_begin, val_end - val_begin);
    i = val_end + 1;
    FieldTrialParameterInterface* field = nullptr;
    for (FieldTrialParameterInterface* field_iter : fields) {
      if (field_iter->Key() == key)
        field = field_iter;
    }
    if (field != nullptr) {
      if (!field->Parse(opt_value)) {
        RTC_DLOG(LS_WARNING)
            << "Failed to read field with key: '" << std::string(key)
            << "' in trial: \"" << std::string(trial_string) << "\"";
      }
    } else {
      RTC_DLOG(LS_INFO) << "No field with key: '" << std::string(key)
                        << "' (found in trial: \"" << std::string(trial_string)
                        << "\")";
    }
  }
}

template <>
absl::optional<bool> ParseTypedParameter<bool>(const string_view str) {
  if (str == "true" || str == "1") {
    return true;
  } else if (str == "false" || str == "0") {
    return false;
  }
  return absl::nullopt;
}

template <>
absl::optional<double> ParseTypedParameter<double>(const string_view str) {
  double value;
  if (sscanf(str.data(), "%lf", &value) == 1) {
    return value;
  } else {
    return absl::nullopt;
  }
}

template <>
absl::optional<int> ParseTypedParameter<int>(const string_view str) {
  int value;
  if (sscanf(str.data(), "%i", &value) == 1) {
    return value;
  } else {
    return absl::nullopt;
  }
}

template <>
absl::optional<std::string> ParseTypedParameter<std::string>(
    const string_view str) {
  return std::string(str);
}

FieldTrialFlag::FieldTrialFlag(const string_view key)
    : FieldTrialFlag(key, false) {}

FieldTrialFlag::FieldTrialFlag(const string_view key, bool default_value)
    : FieldTrialParameterInterface(key), value_(default_value) {}

bool FieldTrialFlag::Get() const {
  return value_;
}

bool FieldTrialFlag::Parse(absl::optional<const string_view> str_value) {
  // Only set the flag if there is no argument provided.
  if (str_value) {
    absl::optional<bool> opt_value = ParseTypedParameter<bool>(*str_value);
    if (!opt_value)
      return false;
    value_ = *opt_value;
  } else {
    value_ = true;
  }
  return true;
}

AbstractFieldTrialEnum::AbstractFieldTrialEnum(const string_view key,
                                               int default_value,
                                               size_t mapping_size)
    : FieldTrialParameterInterface(key),
      value_(default_value),
      mapping_size_(mapping_size) {
  mapping_begin_ = new Pair[mapping_size];
  mapping_end_ = mapping_begin_;
}

AbstractFieldTrialEnum::AbstractFieldTrialEnum(
    const AbstractFieldTrialEnum& other)
    : FieldTrialParameterInterface(other), mapping_size_(other.mapping_size_) {
  mapping_begin_ = new Pair[mapping_size_];
  mapping_end_ = mapping_begin_;
  for (Pair* pair = other.mapping_begin_; pair != other.mapping_end_; ++pair) {
    *(mapping_end_++) = *pair;
  }
}
AbstractFieldTrialEnum::~AbstractFieldTrialEnum() {
  if (mapping_begin_ != nullptr)
    delete mapping_begin_;
}

absl::optional<int> AbstractFieldTrialEnum::TryGet(
    const string_view key) const {
  for (Pair* pair = mapping_begin_; pair != mapping_end_; pair++) {
    if (pair->key == key)
      return pair->value;
  }
  return absl::nullopt;
}

bool AbstractFieldTrialEnum::HasValue(int value) const {
  for (Pair* pair = mapping_begin_; pair != mapping_end_; pair++) {
    if (pair->value == value)
      return true;
  }
  return false;
}

bool AbstractFieldTrialEnum::Parse(
    absl::optional<const string_view> str_value) {
  if (str_value) {
    if (auto value = TryGet(*str_value)) {
      value_ = *value;
      return true;
    }
    absl::optional<int> value = ParseTypedParameter<int>(*str_value);
    if (value.has_value() && HasValue(*value)) {
      value_ = *value;
      return true;
    }
  }
  return false;
}

template class FieldTrialParameter<bool>;
template class FieldTrialParameter<double>;
template class FieldTrialParameter<int>;
template class FieldTrialParameter<std::string>;

template class FieldTrialOptional<double>;
template class FieldTrialOptional<int>;
template class FieldTrialOptional<bool>;
template class FieldTrialOptional<std::string>;

}  // namespace webrtc
