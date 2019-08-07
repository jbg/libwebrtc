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
namespace struct_trial_parser_impl {
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
struct LambdaTypeTraits : public LambdaTypeTraits<decltype(&T::operator())> {};

template <typename ClassType, typename RetType, typename SourceType>
struct LambdaTypeTraits<RetType* (ClassType::*)(SourceType*)const> {
  using ret = RetType;
};
template <typename T>
using return_type_t = typename LambdaTypeTraits<T>::ret;

void ParseConfigParams(
    absl::string_view config_str,
    std::map<std::string, std::function<bool(absl::string_view)>> field_map);

std::string EncodeStringStringMap(std::map<std::string, std::string> mapping);
}  // namespace struct_trial_parser_impl

template <typename StructType>
class StructParametersParser {
 public:
  struct StructParameter {
    std::string key;
    std::function<bool(absl::string_view src, StructType* target)> parse;
    std::function<bool(const StructType& src, const StructType& base)> changed;
    std::function<std::string(const StructType& src)> encode;
  };

  explicit StructParametersParser(std::vector<StructParameter> parameters)
      : parameters_(parameters) {}

  void Parse(absl::string_view src, StructType* target) {
    std::map<std::string, std::function<bool(absl::string_view)>> field_parsers;
    for (const auto& param : parameters_) {
      field_parsers.emplace(param.key, [target, param](absl::string_view src) {
        return param.parse(src, target);
      });
    }
    struct_trial_parser_impl::ParseConfigParams(src, std::move(field_parsers));
  }

  StructType Parse(absl::string_view src) {
    StructType res;
    Parse(src, &res);
    return res;
  }

  std::string EncodeChanged(const StructType& src) {
    static StructType base;
    std::map<std::string, std::string> pairs;
    for (const auto& param : parameters_) {
      if (param.changed(src, base))
        pairs[param.key] = param.encode(src);
    }
    return struct_trial_parser_impl::EncodeStringStringMap(pairs);
  }

  std::string EncodeAll(const StructType& src) {
    std::map<std::string, std::string> pairs;
    for (const auto& param : parameters_) {
      pairs[param.key] = param.encode(src);
    }
    return struct_trial_parser_impl::EncodeStringStringMap(pairs);
  }

  template <
      typename Closure,
      typename T = typename struct_trial_parser_impl::return_type_t<Closure>>
  static StructParameter Field(std::string key, Closure field_getter) {
    return StructParameter{
        key,
        /* parse = */
        [=](absl::string_view src, StructType* target) {
          auto parsed = ParseTypedParameter<T>(std::string(src));
          if (parsed.has_value())
            *field_getter(target) = *parsed;
          return parsed.has_value();
        },
        /* changed = */
        [=](const StructType& src, const StructType& base) {
          T base_value = *field_getter(const_cast<StructType*>(&base));
          T value = *field_getter(const_cast<StructType*>(&src));
          return value != base_value;
        },
        /* encode = */
        [=](const StructType& src) {
          T value = *field_getter(const_cast<StructType*>(&src));
          return struct_trial_parser_impl::StringEncode(value);
        }};
  }

 private:
  std::vector<StructParameter> parameters_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
