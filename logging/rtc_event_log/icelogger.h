/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ICELOGGER_H_
#define LOGGING_RTC_EVENT_LOG_ICELOGGER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "api/optional.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"

namespace webrtc {

class RtcEventLog;

template <bool>
struct IsTerminalEvent;

template <>
struct IsTerminalEvent<true> {
  virtual int terminal_event_type_hash() = 0;
  virtual ~IsTerminalEvent() = default;
};

template <>
struct IsTerminalEvent<false> {};

struct CandidatePairConfigEvent final : public IsTerminalEvent<true> {
  CandidatePairConfigEvent();
  ~CandidatePairConfigEvent() override;
  int terminal_event_type_hash() override;
  enum class Type {
    DESTROYED,
    ADDED,
    UPDATED,
    SELECTED,
  };
  CandidatePairConfigEvent::Type type;
};

struct ConnectivityCheckEvent final : public IsTerminalEvent<true> {
  ConnectivityCheckEvent();
  ~ConnectivityCheckEvent() override;
  int terminal_event_type_hash() override;
  enum class Type {
    CHECK_SENT,
    CHECK_RECEIVED,
    CHECK_RESPONSE_SENT,
    CHECK_RESPONSE_RECEIVED,
  };
  ConnectivityCheckEvent::Type type;
  rtc::Optional<int> rtt;  // Available when a check response received.
};

struct CandidatePairEvent final : public IsTerminalEvent<false> {
  CandidatePairEvent();
  ~CandidatePairEvent();
  enum class Type {
    CONFIG,
    CONNECTIVITY_CHECK,
  };
  CandidatePairEvent::Type type;
  uint32_t candidate_pair_id;
  IceCandidatePairDescription candidate_pair_description;
  // oneof {
  rtc::Optional<CandidatePairConfigEvent> candidate_pair_config_event;
  rtc::Optional<ConnectivityCheckEvent> connectivity_check_event;
  // }
};

// This is the root event in the ICE event hierarchy.
struct IceEvent final : public IsTerminalEvent<false> {
  IceEvent();
  ~IceEvent();
  enum class Type {
    CANDIDATE_PAIR_EVENT,
  };
  int64_t timestamp;
  IceEvent::Type type;
  // Union-like structure or oneof-like structure in protobuf using
  // rtc::Optional. The list below can grow. oneof
  rtc::Optional<CandidatePairEvent> candidate_pair_event;
  // }
  int terminal_event_type_hash;
};

// TODO(qingsi): Consolidate various stats (StunStats, CandidateStats) in the
// network stack and ideally move them to IceEventLog.
struct IceEventStats {
  IceEventStats();
  uint32_t total_checks_sent = 0;
  uint32_t total_checks_received = 0;
  uint32_t total_check_responses_sent = 0;
  uint32_t total_check_responses_received = 0;
  double total_check_rtt_ms = 0;
  double total_check_rtt_ms_squared = 0;
  uint32_t total_candidate_pairs_added_on_wifi = 0;
  uint32_t total_candidate_pairs_added_on_cell = 0;
  uint32_t total_network_switching = 0;
  uint32_t total_network_switching_wifi_to_cell = 0;
  uint32_t total_network_switching_cell_to_wifi = 0;
};

// IceEventLog wraps RtcEventLog and provides structural logging of ICE-specific
// events. The logged events are serialized with other RtcEvent's if protobuf is
// enabled in the build.
class IceEventLog {
 public:
  IceEventLog();
  ~IceEventLog();
  // Utilities to create ICE events at terminal levels in the ICE event
  // hierarchy. Returns the unique pointer to the root-level event, i.e.
  // IceEvent. The arguments in each method signature are the fields required on
  // the path from the root to the leaf event.
  static std::unique_ptr<IceEvent> CreateCandidatePairConfigEvent(
      uint32_t timestamp,
      uint32_t candidate_pair_id,
      const IceCandidatePairDescription& candidate_pair_description,
      CandidatePairConfigEvent::Type type);
  static std::unique_ptr<IceEvent> CreateConnectivityCheckEvent(
      uint32_t timestamp,
      uint32_t candidate_pair_id,
      const IceCandidatePairDescription& candidate_pair_description,
      ConnectivityCheckEvent::Type type,
      rtc::Optional<int> rtt = rtc::nullopt);

  void LogIceEvent(std::unique_ptr<IceEvent> event);
  const IceEventStats& stats() { return stats_; }
  // RtcEventLog is the current infrastructure for log persistence.
  void set_event_log(RtcEventLog* event_log) { event_log_ = event_log; }
  // This method constructs a candidate pair config event for each candidate
  // pair with their description and logs these config events. It is intended to
  // be called when logging starts to ensure that we have at least one config
  // for each candidate pair id in RtcEventLog.
  void DumpCandidatePairDescriptionToRtcEventLog() const;

 private:
  void UpdateStats(const IceEvent& event);
  void LogCandidatePairEvent(std::unique_ptr<IceEvent> event);
  void LogCandidatePairEventToRtcEventLog(const CandidatePairEvent& event);

  RtcEventLog* event_log_ = nullptr;
  std::unordered_map<uint32_t, IceCandidatePairDescription>
      candidate_pair_desc_by_id_;
  std::vector<std::unique_ptr<IceEvent>> candidate_pair_config_events_;
  std::vector<std::unique_ptr<IceEvent>> connectivity_check_events_;
  IceEventStats stats_;
  IceCandidateNetworkType active_network_ = IceCandidateNetworkType::kUnknown;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_ICELOGGER_H_
