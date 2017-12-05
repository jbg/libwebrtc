/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_LOGGING_LOG_IDENTIFIER_H_
#define P2P_LOGGING_LOG_IDENTIFIER_H_

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "p2p/logging/log_event.h"
#include "p2p/logging/log_object.h"
#include "p2p/logging/stringified_enum.h"
#include "p2p/logging/structured_form.h"
#include "rtc_base/basictypes.h"

namespace webrtc {

namespace icelog {

enum class CompareResult { kLess, kGreater, kEqual };

template <typename T>
class Comparable {
 public:
  virtual CompareResult Compare(const T& other) const = 0;
  virtual bool operator<(const T& other) const {
    return Compare(other) == CompareResult::kLess;
  }
  virtual bool operator>(const T& other) const {
    return Compare(other) == CompareResult::kGreater;
  }
  virtual bool operator==(const T& other) const {
    return Compare(other) == CompareResult::kEqual;
  }
  virtual bool operator!=(const T& other) const {
    return Compare(other) != CompareResult::kEqual;
  }
  virtual ~Comparable() = default;
};

// Base class for identifiers in logging.
class LogIdentifier : public LogObject<false>,
                      public Comparable<LogIdentifier> {
 public:
  LogIdentifier();
  explicit LogIdentifier(const std::string& id);
  LogIdentifier(const LogIdentifier& other);

  CompareResult Compare(const LogIdentifier& other) const override;

  std::string id() const { return id_; }

  ~LogIdentifier() override;

  ADD_UNBOXED_DATA_AND_DECLARE_SETTER(std::string, id);
};

// Abstract log sink for serialization.
class LogSink {
 public:
  virtual bool Write(const StructuredForm& data) = 0;
  virtual ~LogSink() = default;
};

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_LOGGING_LOG_IDENTIFIER_H_
