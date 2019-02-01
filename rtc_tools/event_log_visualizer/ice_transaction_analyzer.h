/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_ICE_TRANSACTION_ANALYZER_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_ICE_TRANSACTION_ANALYZER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_tools/event_log_visualizer/ice_transaction.h"
#include "rtc_tools/event_log_visualizer/plot_base.h"

namespace webrtc {

struct IceTransactionAnalyzerConfig {
  int64_t log1_first_timestamp_ms = 0;
  int64_t clock_offset_ms = 0;

  bool use_same_x_axis = false;
  float x_min_s = 0;
  float x_max_s = 0;

  // percentages
  float x_margin = 0.01;
  float y_margin = 0.05;
};

class IceTransactionAnalyzer {
 public:
  IceTransactionAnalyzer(IceTransactionAnalyzerConfig config,
                         const IceTransactions& ice_transactions);

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
  float ToCallTimeSec(IceTimestamp timestamp);
  void SetSuggestedXAxis(Plot* plot, std::string label);

  IceTransactionAnalyzerConfig config_;
  const IceTransactions& ice_transactions_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_ICE_TRANSACTION_ANALYZER_H_
