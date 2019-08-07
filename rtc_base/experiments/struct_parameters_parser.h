/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
#define RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/string_encode.h"

namespace webrtc {
namespace struct_parser_impl {
inline std::string StringEncode(bool val) {
  return rtc::ToString(val);
}
inline std::string StringEncode(double val) {
  return rtc::ToString(val);
}
inline std::string StringEncode(int val) {
  return rtc::ToString(val);
}
inline std::string StringEncode(std::string val) {
  return val;
}
inline std::string StringEncode(DataRate val) {
  return ToString(val);
}
inline std::string StringEncode(DataSize val) {
  return ToString(val);
}
inline std::string StringEncode(TimeDelta val) {
  return ToString(val);
}

template <typename T>
inline std::string StringEncode(absl::optional<T> val) {
  if (val)
    return StringEncode(*val);
  return "";
}

template <typename T>
struct LambdaTraits : public LambdaTraits<decltype(&T::operator())> {};

template <typename ClassType, typename RetType, typename SourceType>
struct LambdaTraits<RetType* (ClassType::*)(SourceType*)const> {
  using ret = RetType;
  using src = SourceType;
};

void ParseConfigParams(
    absl::string_view config_str,
    std::map<std::string, std::function<bool(absl::string_view)>> field_map);

std::string EncodeStringStringMap(std::map<std::string, std::string> mapping);

template <typename StructType>
class StructParameter {
 public:
  explicit StructParameter(absl::string_view key) : key_(key) {}
  const std::string key_;
  virtual bool Parse(absl::string_view src, StructType* target) const = 0;
  virtual bool Changed(const StructType& src, const StructType& base) const = 0;
  virtual std::string Encode(const StructType& src) const = 0;
  virtual ~StructParameter() = default;
};

template <typename StructType, typename Closure, typename T>
class StructParameterImpl : public StructParameter<StructType> {
 public:
  StructParameterImpl(absl::string_view key, Closure&& field_getter)
      : StructParameter<StructType>(key),
        field_getter_(std::forward<Closure>(field_getter)) {}
  bool Parse(absl::string_view src, StructType* target) const override {
    auto parsed = ParseTypedParameter<T>(std::string(src));
    if (parsed.has_value())
      *field_getter_(target) = *parsed;
    return parsed.has_value();
  }
  bool Changed(const StructType& src, const StructType& base) const override {
    T base_value = *field_getter_(const_cast<StructType*>(&base));
    T value = *field_getter_(const_cast<StructType*>(&src));
    return value != base_value;
  }
  std::string Encode(const StructType& src) const override {
    T value = *field_getter_(const_cast<StructType*>(&src));
    return struct_parser_impl::StringEncode(value);
  }

 private:
  Closure field_getter_;
};

}  // namespace struct_parser_impl

template <typename StructType>
class StructParametersParser {
 public:
  using StructParameter = struct_parser_impl::StructParameter<StructType>;
  explicit StructParametersParser(std::vector<StructParameter*> parameters)
      : parameters_(parameters) {}
  ~StructParametersParser() {
    for (StructParameter* param : parameters_) {
      delete param;
    }
  }

  void Parse(StructType* target, absl::string_view src) {
    std::map<std::string, std::function<bool(absl::string_view)>> field_parsers;
    for (const auto& param : parameters_) {
      field_parsers.emplace(param->key_,
                            [target, param](absl::string_view src) {
                              return param->Parse(src, target);
                            });
    }
    struct_parser_impl::ParseConfigParams(src, std::move(field_parsers));
  }

  StructType Parse(absl::string_view src) {
    StructType res;
    Parse(&res, src);
    return res;
  }

  std::string EncodeChanged(const StructType& src) {
    static StructType base;
    std::map<std::string, std::string> pairs;
    for (const auto& param : parameters_) {
      if (param->Changed(src, base))
        pairs[param->key_] = param->Encode(src);
    }
    return struct_parser_impl::EncodeStringStringMap(pairs);
  }

  std::string EncodeAll(const StructType& src) {
    std::map<std::string, std::string> pairs;
    for (const auto& param : parameters_) {
      pairs[param->key_] = param->Encode(src);
    }
    return struct_parser_impl::EncodeStringStringMap(pairs);
  }

 private:
  std::vector<StructParameter*> parameters_;
};
namespace struct_parser {
template <typename Closure,
          typename S = typename struct_parser_impl::LambdaTraits<Closure>::src,
          typename T = typename struct_parser_impl::LambdaTraits<Closure>::ret>
static struct_parser_impl::StructParameter<S>* Field(absl::string_view key,
                                                     Closure&& field_getter) {
  return new struct_parser_impl::StructParameterImpl<S, Closure, T>(
      std::move(key), std::forward<Closure>(field_getter));
}
}  // namespace struct_parser

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
