/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_MULTILOG_ANALYZER_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_MULTILOG_ANALYZER_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_tools/event_log_visualizer/plot_base.h"

namespace webrtc {

// TODO(zstein): LogId struct?

// TODO(zstein): POD struct
// Helper class.
class IceTimestamp final {
 public:
  IceTimestamp(int64_t log_time_us, std::size_t log_id);

  // TODO(zstein): Revisit
  int64_t log_time_us() const { return log_time_us_; }
  std::size_t log_id() const { return log_id_; }

 private:
  int64_t log_time_us_;
  const std::size_t log_id_;
};

// Helper class.
class IceTransaction final {
 public:
  using ConnectionId = std::pair<uint32_t, uint32_t>;
  ConnectionId connection_id() const;

  IceTransaction();

  int stage_reached() const;

  absl::optional<IceTimestamp> start_time() const;
  absl::optional<IceTimestamp> end_time() const;

  void Update(const LoggedIceCandidatePairEvent& event, std::size_t log_id);

  absl::optional<uint32_t> log1_candidate_pair_id;
  absl::optional<uint32_t> log2_candidate_pair_id;

 private:
  absl::optional<IceTimestamp> ping_sent_;
  absl::optional<IceTimestamp> ping_received_;
  absl::optional<IceTimestamp> response_sent_;
  absl::optional<IceTimestamp> response_received_;
};

// Helper class.
struct IceEventWithLogId {
  IceEventWithLogId(LoggedIceCandidatePairEvent event, size_t log_id);

  LoggedIceCandidatePairEvent event;
  const std::size_t log_id;
};

class PlotLimits final {
 public:
  PlotLimits();
  float min() const { return min_; }
  float max() const { return max_; }
  void Update(float value);

 private:
  float min_;
  float max_;
};

struct MultiEventLogAnalyzerConfig {
  MultiEventLogAnalyzerConfig(const ParsedRtcEventLog& first_log,
                              const ParsedRtcEventLog& second_log);

  // For testing.
  MultiEventLogAnalyzerConfig(
      const std::vector<LoggedIceCandidatePairEvent>& log1_events,
      int64_t log1_first_timestamp,
      int64_t log1_last_timestamp,
      const std::vector<LoggedIceCandidatePairEvent>& log2_events,
      int64_t log2_last_timestamp);

  float x_min;
  float x_max;
  int64_t clock_offset;

  float y_margin = 0.05;
};

class MultiEventLogAnalyzer {
 public:
  // TODO(zstein): first_timestamps -> config object
  MultiEventLogAnalyzer(
      MultiEventLogAnalyzerConfig config,
      const std::vector<LoggedIceCandidatePairEvent>& log1_events,
      int64_t log1_first_timestamp,
      const std::vector<LoggedIceCandidatePairEvent>& log2_events,
      int64_t log2_first_timestamp);

  // Builds one plot per candidate pair.
  // Y-axis is client id. Draws a point for each event, connected by transaction
  // id.
  void CreateIceSequenceDiagrams(PlotCollection* plot_collection);

  // Builds one plot per candidate pair.
  // Y-axis is IceCandidatePairEventType. Draws a point for each event,
  // connected by transaction id.
  void CreateIceTransactionGraphs(PlotCollection* plot_collection);

  // TODO(zstein): Per candidate pair version?
  // Builds one plot.
  // Y-axis is transaction state reached (max(IceCandidatePairEventType)). Draws
  // a point for each transaction id.
  void CreateIceTransactionStateGraph(PlotCollection* plot_collection);

  // Builds one plot per candidate pair.
  // Y-axis is transaction RTT. X-axis is time transaction started. Draws a
  // point for each completed transaction.
  void CreateIceTransactionRttGraphs(PlotCollection* plot_collection);

 private:
  using TransactionId = uint32_t;
  void BuildEventsByTransactionId(
      const std::vector<LoggedIceCandidatePairEvent>& log1_events,
      const std::vector<LoggedIceCandidatePairEvent>& log2_events);
  void BuildIceTransactions(
      const std::vector<LoggedIceCandidatePairEvent>& log1_events,
      const std::vector<LoggedIceCandidatePairEvent>& log2_events);

  float FirstTimestamp(size_t log_id);
  float ToCallTimeSec(int64_t timestamp, std::size_t log_id);

  MultiEventLogAnalyzerConfig config_;

  // TODO(zstein): Use log0 and log1 to be consistent with id ints.
  int64_t log1_first_timestamp_;
  int64_t log2_first_timestamp_;

  int64_t clock_offset_;

  using IceEventWithLogIdVec = std::vector<IceEventWithLogId>;
  std::unordered_map<TransactionId, IceEventWithLogIdVec>
      events_by_transaction_id_;
  std::unordered_map<TransactionId, IceTransaction> ice_transactions_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_MULTILOG_ANALYZER_H_
