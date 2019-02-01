/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_ICE_TRANSACTION_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_ICE_TRANSACTION_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc {

// TODO(zstein): LogId struct and use 0/1 vs 1/2 consistently.
const size_t kLogId0 = 0;
const size_t kLogId1 = 1;

struct IceTimestamp final {
  IceTimestamp(int64_t log_time_ms, size_t log_id);

  int64_t log_time_ms;
  const size_t log_id;
};

class IceTransaction final {
 public:
  using ConnectionId = std::pair<uint32_t, uint32_t>;
  ConnectionId connection_id() const;

  IceTransaction();

  int stage_reached() const;

  absl::optional<IceTimestamp> start_time() const;
  absl::optional<IceTimestamp> end_time() const;

  std::vector<absl::optional<IceTimestamp>> timestamps() const;

  void Update(const LoggedIceCandidatePairEvent& event, size_t log_id);

  absl::optional<uint32_t> log1_candidate_pair_id;
  absl::optional<uint32_t> log2_candidate_pair_id;

  absl::optional<IceTimestamp> ping_sent;
  absl::optional<IceTimestamp> ping_received;
  absl::optional<IceTimestamp> response_sent;
  absl::optional<IceTimestamp> response_received;
};

struct IceTransactions final {
  using TransactionId = uint32_t;
  std::unordered_map<uint32_t, IceTransaction> ice_transactions;

  static IceTransactions BuildIceTransactions(
      const std::vector<LoggedIceCandidatePairEvent>& log1_events,
      const std::vector<LoggedIceCandidatePairEvent>& log2_events);
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_ICE_TRANSACTION_H_
