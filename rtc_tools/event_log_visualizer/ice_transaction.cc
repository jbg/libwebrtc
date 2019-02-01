/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/ice_transaction.h"

namespace webrtc {

IceTimestamp::IceTimestamp(int64_t log_time_ms, size_t log_id)
    : log_time_ms(log_time_ms), log_id(log_id) {
  RTC_DCHECK(log_id == kLogId0 || log_id == kLogId1);
}

IceTransaction::IceTransaction() = default;

IceTransaction::ConnectionId IceTransaction::connection_id() const {
  return std::make_pair(log1_candidate_pair_id.value_or(0),
                        log2_candidate_pair_id.value_or(0));
}

int IceTransaction::stage_reached() const {
  if (!ping_sent) {
    return 0;
  } else if (!ping_received) {
    return 1;
  } else if (!response_sent) {
    return 2;
  } else if (!response_received) {
    return 3;
  }
  return 4;
}

absl::optional<IceTimestamp> IceTransaction::start_time() const {
  return ping_sent;
}

absl::optional<IceTimestamp> IceTransaction::end_time() const {
  return response_received;
}

void IceTransaction::Update(const LoggedIceCandidatePairEvent& event,
                            size_t log_id) {
  IceTimestamp timestamp(event.log_time_ms(), log_id);

  switch (event.type) {
    case IceCandidatePairEventType::kCheckSent:
      RTC_DCHECK(!ping_sent);
      ping_sent.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kCheckReceived:
      RTC_DCHECK(!ping_received);
      ping_received.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kCheckResponseSent:
      RTC_DCHECK(!response_sent);
      response_sent.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kCheckResponseReceived:
      RTC_DCHECK(!response_received);
      response_received.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kNumValues:
      RTC_NOTREACHED();
      break;
  }
}

std::vector<absl::optional<IceTimestamp>> IceTransaction::timestamps() const {
  return std::vector<absl::optional<IceTimestamp>>{
      ping_sent, ping_received, response_sent, response_received};
}

IceTransactions IceTransactions::BuildIceTransactions(
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events) {
  IceTransactions transactions;
  for (const auto& event : log1_events) {
    IceTransaction& transaction =
        transactions.ice_transactions[event.transaction_id];
    transaction.log1_candidate_pair_id = event.candidate_pair_id;
    transaction.Update(event, kLogId0);
  }
  for (const auto& event : log2_events) {
    IceTransaction& transaction =
        transactions.ice_transactions[event.transaction_id];
    transaction.log2_candidate_pair_id = event.candidate_pair_id;
    transaction.Update(event, kLogId1);
  }
  return transactions;
}

}  // namespace webrtc
