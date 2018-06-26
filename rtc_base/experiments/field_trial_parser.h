/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
#define RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_

#include <stdint.h>
#include <initializer_list>
#include <string>
#include "absl/types/optional.h"

// Field trial parser functionality. Provides funcitonality to parse field trial
// argument strings in key:value format. Each parameter is described using
// key:value, parameters are separated with a ,. Values can't include the comma
// character, since there's no quote facility. For most types, white space is
// ignored. Parameters are declared with a given type for which an
// implementation of ParseTypedParameter should be provided. The
// ParseTypedParameter implementation is given whatever is between the : and the
// ,. FieldTrialOptional will use nullopt if the key is provided without :.

// Example string: "my_optional,my_int:3,my_string:hello"

// For further description of usage and behavior, see the examples in the unit
// tests.

namespace webrtc {
class string_view {
 public:
  string_view() = default;
  string_view(const char* data, size_t length);
  string_view(const char* c_str);
  string_view(const std::string& string);  // Note: highly unsafe.
  ~string_view() = default;

  string_view(const string_view& string) = default;
  string_view& operator=(const string_view&) = default;
  operator std::string() const { return std::string(data_, 0, size_); }

  bool operator==(string_view other) const;
  char operator[](int index) const { return *(data_ + index); }
  const char* data() const { return data_; }
  size_t length() const { return size_; }
  bool empty() const { return size_ == 0; }
  string_view substr(int pos, int len) const;

 private:
  const char* data_;
  size_t size_;
};

class FieldTrialParameterInterface {
 public:
  virtual ~FieldTrialParameterInterface();

 protected:
  explicit FieldTrialParameterInterface(const string_view key);
  friend void ParseFieldTrial(
      std::initializer_list<FieldTrialParameterInterface*> fields,
      const string_view raw_string);
  virtual bool Parse(absl::optional<const string_view> str_value) = 0;
  const string_view Key() const;

 private:
  const string_view key_;
};

// ParseFieldTrial function parses the given string and fills the given fields
// with extrated values if available.
void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    const string_view raw_string);

// Specialize this in code file for custom types. Should return absl::nullopt if
// the given string cannot be properly parsed.
template <typename T>
absl::optional<T> ParseTypedParameter(const string_view);

// This class uses the ParseTypedParameter function to implement a parameter
// implementation with an enforced default value.
template <typename T>
class FieldTrialParameter : public FieldTrialParameterInterface {
 public:
  FieldTrialParameter(const string_view key, T default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  T Get() const { return value_; }
  operator T() const { return Get(); }

 protected:
  bool Parse(absl::optional<const string_view> str_value) override {
    if (str_value) {
      absl::optional<T> value = ParseTypedParameter<T>(*str_value);
      if (value.has_value()) {
        value_ = value.value();
        return true;
      }
    }
    return false;
  }

 private:
  T value_;
};

class AbstractFieldTrialEnum : public FieldTrialParameterInterface {
 public:
  struct Pair {
    string_view key;
    int value;
  };
  AbstractFieldTrialEnum(const string_view key,
                         int default_value,
                         size_t mapping_size);
  ~AbstractFieldTrialEnum() override;
  AbstractFieldTrialEnum(const AbstractFieldTrialEnum&);

 protected:
  bool Parse(absl::optional<const string_view> str_value) override;

 protected:
  absl::optional<int> TryGet(const string_view key) const;
  bool HasValue(int value) const;
  int value_;

  Pair* mapping_begin_;
  Pair* mapping_end_;
  size_t mapping_size_;
};

// The FieldTrialEnum class can be used to quickly define a parser for a
// specific enum. It handles values provided as integers and as strings if a
// mapping is provided.
template <typename T>
class FieldTrialEnum : public AbstractFieldTrialEnum {
 public:
  struct Pair {
    string_view key;
    T value;
  };
  FieldTrialEnum(const string_view key,
                 T default_value,
                 std::initializer_list<Pair> mapping)
      : AbstractFieldTrialEnum(key,
                               static_cast<int>(default_value),
                               mapping.size()) {
    for (const auto& it : mapping) {
      *(mapping_end_++) = {it.key, static_cast<int>(it.value)};
    }
  }
  T Get() const { return static_cast<T>(value_); }
  operator T() const { return Get(); }
};

// This class uses the ParseTypedParameter function to implement an optional
// parameter implementation that can default to absl::nullopt.
template <typename T>
class FieldTrialOptional : public FieldTrialParameterInterface {
 public:
  explicit FieldTrialOptional(const string_view key)
      : FieldTrialParameterInterface(key) {}
  FieldTrialOptional(const string_view key, absl::optional<T> default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  absl::optional<T> Get() const { return value_; }

 protected:
  bool Parse(absl::optional<const string_view> str_value) override {
    if (str_value) {
      absl::optional<T> value = ParseTypedParameter<T>(*str_value);
      if (!value.has_value())
        return false;
      value_ = value.value();
    } else {
      value_ = absl::nullopt;
    }
    return true;
  }

 private:
  absl::optional<T> value_;
};

// Equivalent to a FieldTrialParameter<bool> in the case that both key and value
// are present. If key is missing, evaluates to false. If key is present, but no
// explicit value is provided, the flag evaluates to true.
class FieldTrialFlag : public FieldTrialParameterInterface {
 public:
  explicit FieldTrialFlag(const string_view key);
  FieldTrialFlag(const string_view key, bool default_value);
  bool Get() const;

 protected:
  bool Parse(absl::optional<const string_view> str_value) override;

 private:
  bool value_;
};

// Accepts true, false, else parsed with sscanf %i, true if != 0.
extern template class FieldTrialParameter<bool>;
// Interpreted using sscanf %lf.
extern template class FieldTrialParameter<double>;
// Interpreted using sscanf %i.
extern template class FieldTrialParameter<int>;
// Using the given value as is.
extern template class FieldTrialParameter<std::string>;

extern template class FieldTrialOptional<double>;
extern template class FieldTrialOptional<int>;
extern template class FieldTrialOptional<bool>;
extern template class FieldTrialOptional<std::string>;

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
