/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_LOGGING_LOG_OBJECT_H_
#define P2P_LOGGING_LOG_OBJECT_H_

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "p2p/logging/stringified_enum.h"
#include "p2p/logging/structured_form.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace icelog {

// When defining objects on top of the StructuredForm, customized internal data
// should be defined using the following macro and data mutation should be
// done via the setter. The internal data is likely not boxed in the value of
// the underlying StructuredForm, which can cause information missing in
// serialization and data transfer using the infrastructure provided by the
// StructuredForm. To box and synchronize the internal data in the
// StructuredForm in a correct way, use the HasUnboxedInternalData interface
// defined below to reinforce boxing in construction and implement the setters
// defined here to sync the data with boxed version.
#define ADD_UNBOXED_DATA_WITHOUT_SETTER(type, name) \
 private:                                           \
  type name##_;

#define ADD_UNBOXED_DATA_AND_DECLARE_SETTER(type, name) \
 private:                                               \
  type name##_;                                         \
                                                        \
 public:                                                \
  void set_##name(type const& value);

// The HasUnboxedInternalData interface is to reinforce the structure of a
// LogObject (defined below) as a StructuredForm when it has customized data
// that are statically defined in the compile time via member declaration, e.g.
// the creation timestamp of a log event or the starting timestamp of the valid
// period of an event hook have their own internal data. The intention is to
// remind a user to box the necessary internal data in constructor to the
// underlying StructuredForm for easy data representation and transferring (as a
// Describable).
//
// In addition, each LogObject can have a child StructuredForm with key "data"
// to hold any customized data defined in the runtime by calling the method
// AddData (see definition of LogObject).
template <bool has_unboxed_internal_data>
struct HasUnboxedInternalData;

template <>
struct HasUnboxedInternalData<true> {
  virtual void BoxInternalDataInConstructor() = 0;
  virtual ~HasUnboxedInternalData() = default;
};

template <>
struct HasUnboxedInternalData<false> {};

template <bool has_unboxed_internal_data = true>
class LogObject : public StructuredForm,
                  public HasUnboxedInternalData<has_unboxed_internal_data> {
 public:
  LogObject() = default;
  explicit LogObject(const std::string key) : StructuredForm(key) {}
  LogObject(const LogObject& other) : StructuredForm(other) {}

  /*virtual*/ LogObject& operator=(const LogObject& other) {
    StructuredForm::operator=(other);
    return *this;
  }

  // Creates a child StructuredForm with key data_section_key if it does not
  // exists to hold internal data defined in runtime, and then appends a
  // grandchild - created with the given key with the given string value - to
  // the data child.
  void AddData(const std::string& data_key,
               const std::string& data_value,
               bool, /*not used for string data value*/
               const std::string& data_section_key = "data") {
    AddDataImpl<std::string>(data_key, data_value, false, data_section_key);
  }

  // A similar method as above for the case when the data value is a
  // StructuredForm. If reduce_level is true, the key of the StructuredForm
  // holding the value is replaced by the given key to reduce the level of
  // hierarchy. E.g, using the JSON notation, after adding a piece of data
  // {'timestamp': 1} with the data_key 'Timestamp' to a LogObject, the
  // LogObject is represented as
  //  1) if reduce_level = true,
  //     {'key_of_the_object':
  //       [ // set of child StructuredForm
  //         {'data':
  //           {'Timestamp':
  //             {'timestamp': 1}
  //           },
  //           // ...other dynamically defined data grandchild StructuredForm's
  //         },
  //         // ...other statically defined child StructuredForm's
  //       ]
  //     }
  //  2) if reduce_level = false,
  //     {'key_of_the_object':
  //       [ // set of child StructuredForm
  //         {'data':
  //           {'Timestamp': 1}
  //           // ...other dynamically defined data grandchild StructuredForm's
  //         },
  //         // ...other statically defined child StructuredForm's
  //       ]
  //     }
  void AddData(const std::string& data_key,
               const StructuredForm& data_value,
               bool reduce_level = false,
               const std::string& data_section_key = "data") {
    AddDataImpl<StructuredForm>(data_key, data_value, reduce_level,
                                data_section_key);
  }

  ~LogObject() = default;

 private:
  template <typename data_value_type>
  void AddDataImpl(const std::string& data_key,
                   const data_value_type& data_value,
                   bool reduce_level = false,
                   const std::string& data_section_key = "data") {
    StructuredForm data_grandchild(data_key);
    if (reduce_level) {
      data_grandchild = StructuredForm(data_value);
      data_grandchild.set_key(data_key);
    } else {
      data_grandchild.SetValue(data_value);
    }

    StructuredForm* data_child = GetChildWithKey(data_section_key);
    if (data_child == nullptr) {
      // First time adding dynamic data to the data section "data_section_key".
      StructuredForm new_data_section_sf(data_section_key);
      AddChild(new_data_section_sf);
      data_child = GetChildWithKey(data_section_key);
      RTC_DCHECK(data_child != nullptr);
    }
    data_child->AddChild(data_grandchild);
  }
};

// DEFINE_ENUMERATED_LOG_OBJECT defines an LogObject-derived type with a
// build-in stringified enum type (see stringifiedenum.h). "enum_name"
// will be the type name and enum_key will be the key for the underlying
// LogObject when it is treated as a key-value pair based StructuredForm.
// Refer to the definition of StructuredForm below.

#define DEFINE_ENUMERATED_LOG_OBJECT(enum_name, enum_key, ...)              \
  DEFINE_STRINGIFIED_ENUM(enum_name##Base, __VA_ARGS__);                    \
  class enum_name final : public enum_name##Base, public LogObject<false> { \
   public:                                                                  \
    enum_name() {}                                                          \
    explicit enum_name(Value enum_val)                                      \
        : enum_name##Base(enum_val), LogObject(#enum_key) {                 \
      std::string enum_val_str = EnumToStr(enum_val);                       \
      if (enum_val == enum_name##Base::Value::kUndefined) {                 \
        value_sf_set_["value"].reset(new StructuredForm("undefined"));      \
        value_sf_set_["comment"].reset(new StructuredForm(enum_val_str));   \
        return;                                                             \
      }                                                                     \
      value_str_ = enum_val_str;                                            \
    }                                                                       \
    enum_name(const enum_name& other)                                       \
        : enum_name##Base(other), LogObject(other) {}                       \
    ~enum_name() final = default;                                           \
  };

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_LOGGING_LOG_OBJECT_H_
