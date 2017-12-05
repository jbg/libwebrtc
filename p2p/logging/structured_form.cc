/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/logging/structured_form.h"

#include <sstream>

#include "rtc_base/basictypes.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

namespace icelog {

// Start the implementation of StructuredForm.
StructuredForm::StructuredForm()
    : key_(std::string()), value_str_(std::string()), is_stump_(true) {
  value_sf_set_.clear();
  child_keys_.clear();
}

StructuredForm::StructuredForm(const std::string key)
    : key_(key), value_str_(std::string()), is_stump_(true) {
  value_sf_set_.clear();
  child_keys_.clear();
}

StructuredForm::StructuredForm(const StructuredForm& other) {
  Init(other);
}

void StructuredForm::Init(const StructuredForm& other) {
  // Deep copy.
  key_ = other.key_;
  value_str_ = other.value_str_;
  value_sf_set_.clear();
  if (!other.value_sf_set_.empty()) {
    for (auto it = other.value_sf_set_.begin(); it != other.value_sf_set_.end();
         ++it) {
      value_sf_set_[it->first].reset(new StructuredForm(*(it->second)));
    }
  }
  child_keys_ = other.child_keys_;
  is_stump_ = other.is_stump_;
}

StructuredForm& StructuredForm::operator=(const StructuredForm& other) {
  Init(other);
  return *this;
}

bool StructuredForm::operator==(const StructuredForm& other) const {
  if (key_ != other.key_ || value_str_ != other.value_str_ ||
      child_keys_ != other.child_keys_) {
    return false;
  }

  for (const std::string& child_key : child_keys_) {
    RTC_DCHECK(value_sf_set_.find(child_key) != value_sf_set_.end());
    if (other.value_sf_set_.find(child_key) == other.value_sf_set_.end()) {
      return false;
    }
    if (*value_sf_set_.at(child_key) != *other.value_sf_set_.at(child_key)) {
      return false;
    }
  }
  return true;
}

bool StructuredForm::operator!=(const StructuredForm& other) const {
  return !operator==(other);
}

StructuredForm StructuredForm::SetValueAsString(const std::string& value_str) {
  StructuredForm original = *this;
  value_str_ = value_str;
  value_sf_set_.clear();
  child_keys_.clear();
  is_stump_ = true;
  return original;
}

StructuredForm StructuredForm::SetValueAsStructuredForm(
    const StructuredForm& child) {
  StructuredForm original = *this;
  value_str_ = std::string();
  value_sf_set_.clear();
  value_sf_set_[child.key_].reset(new StructuredForm(child));
  child_keys_.insert(child.key_);
  is_stump_ = false;
  return original;
}

bool StructuredForm::HasChildWithKey(const std::string child_key) const {
  bool ret = child_keys_.find(child_key) != child_keys_.end();
  RTC_DCHECK(ret == (value_sf_set_.find(child_key) != value_sf_set_.end()));
  return ret;
}

void StructuredForm::AddChild(const StructuredForm& child) {
  value_str_ = std::string();
  is_stump_ = false;
  value_sf_set_[child.key_].reset(new StructuredForm(child));
  child_keys_.insert(child.key_);
}

bool StructuredForm::UpdateChild(const StructuredForm& child) {
  if (!HasChildWithKey(child.key_)) {
    return false;
  }
  AddChild(child);
  return true;
}

StructuredForm* StructuredForm::GetChildWithKey(
    const std::string& child_key) const {
  if (!HasChildWithKey(child_key)) {
    return nullptr;
  }
  return value_sf_set_.at(child_key).get();
}

bool StructuredForm::IsStump() const {
  return is_stump_;
}

bool StructuredForm::IsNull() const {
  return operator==(kNullStructuredForm);
}

// Generates JSON-parsable string representation of a StructuredForm.
std::string StructuredForm::ToString() const {
  std::stringstream ss;
  std::string ret;
  ss << "{"
     << "\"" << key_ << "\":";
  if (IsStump()) {
    ss << "\"" << value_str_ << "\"}";
    ret = ss.str();
  } else {
    ss << "{";
    for (const std::string& child_key : child_keys_) {
      RTC_DCHECK(HasChildWithKey(child_key));
      std::string child_sf_str = value_sf_set_.at(child_key)->ToString();
      ss << child_sf_str.substr(1, child_sf_str.size() - 2) << ",";
    }
    ret = ss.str();
    ret.pop_back();
    ret += "}}";
  }
  return ret;
}

StructuredForm StructuredForm::ToStructuredForm() const {
  return *this;
}

StructuredForm::~StructuredForm() {}

}  // namespace icelog

}  // namespace webrtc
