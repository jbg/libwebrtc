/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_LOGGING_STRUCTURED_FORM_H_
#define P2P_LOGGING_STRUCTURED_FORM_H_

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "p2p/logging/stringified_enum.h"

namespace webrtc {

namespace icelog {

// Start the definition of StructuredForm.

// The Describable interface prescribes the available conversion of data
// representation of an object for data transfer and human readability.
class StructuredForm;

class Describable {
 public:
  Describable() = default;
  virtual std::string ToString() const = 0;
  virtual StructuredForm ToStructuredForm() const = 0;
  virtual ~Describable() = default;
};

// StructuredForm is an abstraction on top of the underlying structured format.
// Currently a native implementation is provided but it can be used as an API
// layer for logging.
//
// A StructuredForm is defined as a key-value pair, where
//  1) the key is a string and
//  2) the value is a string or a set of StructuredForm's
// A StructuredForm is a stump if its value is a string, or otherwise it has
// children StructuredForm's in its value.
class StructuredForm : public Describable {
 public:
  StructuredForm();
  explicit StructuredForm(const std::string key);
  StructuredForm(const StructuredForm& other);

  virtual StructuredForm& operator=(const StructuredForm& other);

  virtual bool operator==(const StructuredForm& other) const;
  virtual bool operator!=(const StructuredForm& other) const;

  // The following methods implement data operations on a StructuredForm using
  // the underlying data representation infrastructure (jsoncpp currently) and
  // maintain the conceptual structure of a StructuredForm described above.

  // Replaces any existing value with the given string. The
  // original structured form before value setting is returned.
  StructuredForm SetValueAsString(const std::string& value_str);

  // Replaces any existing value with the given StructuredForm.
  // The original structured form before value setting is return.
  StructuredForm SetValueAsStructuredForm(const StructuredForm& child);

  StructuredForm SetValue(const std::string& value) {
    return SetValueAsString(value);
  }

  StructuredForm SetValue(const StructuredForm& value) {
    return SetValueAsStructuredForm(value);
  }

  // Returns true if a child StructuredForm with the given key exists in the
  // value and false otherwise.
  bool HasChildWithKey(const std::string child_key) const;

  // Add a child StructuredForm if there is no existing child with the same key
  // and clear any string value if the parent is a stump, or otherwise replaces
  // the value of the child with same key.
  //
  // The caller should check for the side effect, using IsStump(), in case the
  // string value should not be cleared if the parent is a stump.
  void AddChild(const StructuredForm& child);

  // Returns false if there is no existing child with the same key, or otherwise
  // replaces the existing child with the same key and return true.
  bool UpdateChild(const StructuredForm& child);

  // Returns a copy of a child StructuredForm with the given key in the value if
  // it exists and otherwise a kNullStructuredForm is returned.
  StructuredForm* GetChildWithKey(const std::string& child_key) const;

  bool IsStump() const;
  bool IsNull() const;

  std::string key() const { return key_; }
  void set_key(const std::string& key) { key_ = key; }

  // The keys of children can be used to iterate child StructuredForm's but a
  // better approach is to define a new iterator instead of
  // implementation-specific iteration using string keys.
  std::unordered_set<std::string> child_keys() const { return child_keys_; }

  // From Describable.
  /*virtual*/ std::string ToString() const override;
  /*virtual*/ StructuredForm ToStructuredForm() const override;

  void Serialize() const;

  ~StructuredForm() override;

 protected:
  std::string key_;
  std::string value_str_;
  std::unordered_map<std::string /*child_key*/,
                     std::unique_ptr<StructuredForm> /*child_sf*/>
      value_sf_set_;
  std::unordered_set<std::string> child_keys_;

 private:
  void Init(const StructuredForm& other);
  bool is_stump_;
};

const StructuredForm kNullStructuredForm("null");

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_LOGGING_STRUCTURED_FORM_H_
