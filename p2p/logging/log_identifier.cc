/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>

#include "p2p/logging/log_identifier.h"

#include "p2p/logging/log_event.h"
#include "p2p/logging/log_object.h"
#include "p2p/logging/structured_form.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace icelog {

LogIdentifier::LogIdentifier() : LogObject("id") {}
LogIdentifier::LogIdentifier(const LogIdentifier& other)
    : LogObject(other), id_(other.id()) {}
LogIdentifier::LogIdentifier(const std::string& id) : LogObject("id") {
  // Note that the id string may contain characters that should be escaped
  // for parsing in postprocessing, depending on the implementation of the
  // structured form.
  set_id(id);
}

// From ADD_UNBOXED_DATA_AND_DECLARE_SETTER.
void LogIdentifier::set_id(const std::string& id) {
  id_ = id;
  // Boxing.
  SetValue(id_);
}

CompareResult LogIdentifier::Compare(const LogIdentifier& other) const {
  std::string this_id = id();
  std::string other_id = other.id();
  if (this_id < other_id) {
    return CompareResult::kLess;
  } else if (this_id > other_id) {
    return CompareResult::kGreater;
  }
  return CompareResult::kEqual;
}

LogIdentifier::~LogIdentifier() {}

}  // namespace icelog

}  // namespace webrtc
