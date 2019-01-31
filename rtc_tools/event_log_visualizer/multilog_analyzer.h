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

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_tools/event_log_visualizer/plot_base.h"

namespace webrtc {

// TODO(zstein): Decide on using log1 and log2 vs log_id=0 and log_id=1
// and use consistently throughout.
// TODO(zstein): LogId struct?

// Helper class.
struct IceTimestamp final {
  IceTimestamp(int64_t log_time_ms, size_t log_id);

  int64_t log_time_ms;
  const size_t log_id;
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

  void Update(const LoggedIceCandidatePairEvent& event, size_t log_id);

  absl::optional<uint32_t> log1_candidate_pair_id;
  absl::optional<uint32_t> log2_candidate_pair_id;

  std::vector<absl::optional<IceTimestamp>> timestamps() const;

 private:
  absl::optional<IceTimestamp> ping_sent_;
  absl::optional<IceTimestamp> ping_received_;
  absl::optional<IceTimestamp> response_sent_;
  absl::optional<IceTimestamp> response_received_;
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
  int64_t log1_first_timestamp_ms = 0;
  int64_t clock_offset_ms = 0;

  bool use_same_x_axis = false;
  float x_min_s = 0;
  float x_max_s = 0;

  // percentages
  float x_margin = 0.01;
  float y_margin = 0.05;
};

class MultiEventLogAnalyzer {
 public:
  MultiEventLogAnalyzer(
      MultiEventLogAnalyzerConfig config,
      const std::vector<LoggedIceCandidatePairEvent>& log1_events,
      const std::vector<LoggedIceCandidatePairEvent>& log2_events);

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
  void BuildIceTransactions(
      const std::vector<LoggedIceCandidatePairEvent>& log1_events,
      const std::vector<LoggedIceCandidatePairEvent>& log2_events);

  float ToCallTimeSec(IceTimestamp timestamp);

  void SetSuggestedXAxis(Plot* plot,
                         const PlotLimits& limits,
                         std::string label);

  MultiEventLogAnalyzerConfig config_;

  std::unordered_map<TransactionId, IceTransaction> ice_transactions_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_MULTILOG_ANALYZER_H_
