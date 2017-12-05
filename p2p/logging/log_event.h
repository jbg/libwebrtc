/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_LOGGING_LOG_EVENT_H_
#define P2P_LOGGING_LOG_EVENT_H_

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "p2p/logging/log_object.h"
#include "p2p/logging/stringified_enum.h"
#include "p2p/logging/structured_form.h"

namespace webrtc {

namespace icelog {

// LogEvent and LogEventPool.
DEFINE_ENUMERATED_LOG_OBJECT(LogEventType,
                             type,
                             kNone,
                             kAny,
                             kCandidateGathered,
                             kConnectionCreated,
                             kConnectionStateChanged,
                             kStunBindRequestSent,
                             kStunBindRequestResponseReceived,
                             kConnectionReselected,
                             kNumLogEventTypes);

class LogHook;
using Timestamp = uint64_t;

class LogEvent final : public LogObject<true> {
 public:
  explicit LogEvent(const LogEventType& type);
  LogEvent(const LogEvent& other);

  /*virtual*/ LogEvent& operator=(const LogEvent& other);

  // From HasUnboxedInternalData.
  /*virtual*/ void BoxInternalDataInConstructor() override;

  void AddHookForDownstreamEvents(const LogHook& hook);
  void UpdateUpstreamEvents();
  // The event-hook attachment is based on the matching of signature that
  // consists of a set of key-value pairs.
  // The signature value can be either a string or a StructuredForm.
  template <typename signature_value_type>
  void AddSignatureForUpstreamHook(
      const std::string& signature_key,
      const signature_value_type& signature_value) {
    AddData(signature_key, signature_value, false, "signature");
  }

  std::string id() const { return id_; }
  LogEventType::Value type() const { return type_.value(); }
  Timestamp created_at() const { return event_created_at_; }
  std::unordered_set<LogEvent*> upstream_events() const {
    return upstream_events_;
  }

  ~LogEvent() final;

  // Internal customized data.
  ADD_UNBOXED_DATA_WITHOUT_SETTER(std::string, id);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(Timestamp, event_created_at);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(LogEventType, type);
  ADD_UNBOXED_DATA_AND_DECLARE_SETTER(std::unordered_set<LogEvent*>,
                                      upstream_events);

 private:
  void Init(const LogEvent& other);
};

class LogEventPool {
 public:
  // Singleton, constructor and destructor private
  static LogEventPool* Instance();
  // store the event and return a pointer to the stored copy
  LogEvent* RegisterEvent(const LogEvent& event);

 private:
  LogEventPool();
  ~LogEventPool();
  std::deque<LogEvent> internal_event_pool_;
};

// LogHook and LogHookPool.
class LogHook final : public LogObject<true> {
 public:
  LogHook() = delete;
  LogHook(const LogHook& other);
  explicit LogHook(LogEvent* originating_event,
                   LogEventType::Value downstream_event_type);

  // From HasUnboxedInternalData.
  /*virtual*/ void BoxInternalDataInConstructor() override;

  // The signature can be matched to an event signature added by
  // LogEvent::AddSignatureForUpstreamHook for attachment.
  template <typename signature_value_type>
  void AddSignatureForDownstreamEvent(
      const std::string& signature_key,
      const signature_value_type& signature_value) {
    AddData(signature_key, signature_value, false, "signature");
  }
  bool CanBeAttachedByDownstreamEvent(const LogEvent& event) const;

  Timestamp valid_from() const { return hook_valid_from_; }
  LogEvent* originating_event() const { return originating_event_; }
  LogEventType::Value downstream_event_type() const {
    return downstream_event_type_;
  }

  ~LogHook() final;

  // Internal customized data.
  ADD_UNBOXED_DATA_WITHOUT_SETTER(Timestamp, hook_valid_from);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(LogEvent*, originating_event);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(LogEventType::Value, downstream_event_type);

 private:
  void Init(const LogHook& other);

 public:
  size_t HashCode() const;
  // Customized hasher.
  struct Hasher {
    size_t operator()(const LogHook& hook) const;
  };
  // Customized comparator.
  // The event hook is not defined as comparable since we only define so far a
  // binary predicate of the equality between two hooks.
  struct Comparator {
    bool operator()(const LogHook& lhs, const LogHook& rhs) const;
  };
};

class LogHookPool {
 public:
  // Singleton, constructor and destructor private.
  static LogHookPool* Instance();

  void RegisterEventHook(const LogHook& hook);
  std::unordered_set<LogEvent*> GetUpstreamEventsForAnEvent(
      const LogEvent& event) const;

 private:
  LogHookPool();
  ~LogHookPool();
  std::unordered_set<LogHook, LogHook::Hasher, LogHook::Comparator>
      internal_hook_pool_;
};

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_LOGGING_LOG_EVENT_H_
