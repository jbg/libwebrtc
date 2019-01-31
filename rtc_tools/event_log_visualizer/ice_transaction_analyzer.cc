/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/ice_transaction_analyzer.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "rtc_base/logging.h"
#include "rtc_tools/event_log_visualizer/clock_offset_calculator.h"

namespace webrtc {

namespace {

const int kNumMillisecsPerSec = 1000;
const size_t kLogId0 = 0;
const size_t kLogId1 = 1;

bool CompareTimeSeriesPoint(const TimeSeriesPoint& lhs,
                            const TimeSeriesPoint& rhs) {
  return lhs.x < rhs.x;
}

std::string ConnectionIdString(const IceTransaction::ConnectionId& id) {
  return std::to_string(id.first) + ", " + std::to_string(id.second);
}

std::string JoinCandidatePairIds(const std::unordered_set<uint32_t>& ids) {
  rtc::StringBuilder sb;
  for (auto itr = ids.begin(); itr != ids.end();) {
    sb << *itr;
    if (++itr != ids.end()) {
      sb << ", ";
    }
  }
  return sb.Release();
}

}  // namespace

PlotLimits::PlotLimits()
    : min_(std::numeric_limits<float>::max()),
      max_(-std::numeric_limits<float>::max()) {}

void PlotLimits::Update(float value) {
  min_ = std::min(min_, value);
  max_ = std::max(max_, value);
}

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
  if (!ping_sent_) {
    return 0;
  } else if (!ping_received_) {
    return 1;
  } else if (!response_sent_) {
    return 2;
  } else if (!response_received_) {
    return 3;
  }
  return 4;
}

absl::optional<IceTimestamp> IceTransaction::start_time() const {
  return ping_sent_;
}

absl::optional<IceTimestamp> IceTransaction::end_time() const {
  return response_received_;
}

void IceTransaction::Update(const LoggedIceCandidatePairEvent& event,
                            size_t log_id) {
  IceTimestamp timestamp(event.log_time_ms(), log_id);

  switch (event.type) {
    case IceCandidatePairEventType::kCheckSent:
      RTC_DCHECK(!ping_sent_);
      ping_sent_.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kCheckReceived:
      RTC_DCHECK(!ping_received_);
      ping_received_.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kCheckResponseSent:
      RTC_DCHECK(!response_sent_);
      response_sent_.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kCheckResponseReceived:
      RTC_DCHECK(!response_received_);
      response_received_.emplace(event.log_time_ms(), log_id);
      break;
    case IceCandidatePairEventType::kNumValues:
      RTC_NOTREACHED();
      break;
  }
}

std::vector<absl::optional<IceTimestamp>> IceTransaction::timestamps() const {
  return std::vector<absl::optional<IceTimestamp>>{
      ping_sent_, ping_received_, response_sent_, response_received_};
}

IceTransactionAnalyzer::IceTransactionAnalyzer(
    IceTransactionAnalyzerConfig config,
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events)
    : config_(config) {
  BuildIceTransactions(log1_events, log2_events);
}

// TODO(zstein): De-duplicate some of the code shared with
// CreateIceTransactionGraphs
void IceTransactionAnalyzer::CreateIceSequenceDiagrams(
    PlotCollection* plot_collection) {
  using CandidatePairId = uint32_t;
  // CandidatePairIds from different logs should share the same plot if they
  // share a transaction id.
  std::unordered_map<CandidatePairId, Plot*> plots;
  std::unordered_map<Plot*, PlotLimits> plot_x_limits;

  // Used to set plot title. Needs to happen after all transaction ids have
  // been processed since a transaction can be missing one side's candidate pair
  // id.
  std::unordered_map<Plot*, std::unordered_set<CandidatePairId>> plot_ids;

  for (const auto& p : ice_transactions_) {
    TransactionId transaction_id = p.first;
    const IceTransaction& transaction = p.second;
    TimeSeries time_series(std::to_string(transaction_id), LineStyle::kLine,
                           PointStyle::kHighlight);
    std::set<CandidatePairId> candidate_pair_ids;
    if (transaction.log1_candidate_pair_id) {
      candidate_pair_ids.insert(*transaction.log1_candidate_pair_id);
    }
    if (transaction.log2_candidate_pair_id) {
      candidate_pair_ids.insert(*transaction.log2_candidate_pair_id);
    }
    bool has_non_check_msg = false;
    auto timestamps = transaction.timestamps();
    for (size_t i = 0; i < timestamps.size(); ++i) {
      if (!timestamps[i]) {
        continue;
      }
      auto ice_timestamp = *timestamps[i];
      float x = ToCallTimeSec(ice_timestamp);
      float y = ice_timestamp.log_id;
      has_non_check_msg |=
          i != static_cast<size_t>(IceCandidatePairEventType::kCheckSent);
      time_series.points.emplace_back(x, y);
    }

    // Don't plot if we don't have at least one non-check message.
    if (!has_non_check_msg) {
      continue;
    }

    Plot* plot;
    for (auto candidate_pair_id : candidate_pair_ids) {
      plot = plots[candidate_pair_id];
      if (plot) {
        break;
      }
    }
    if (plot == nullptr) {
      plot = plot_collection->AppendNewPlot();
      plot->SetYAxis(0, 1, "Client", config_.y_margin, config_.y_margin);
      plot_x_limits[plot];
    }
    for (auto candidate_pair_id : candidate_pair_ids) {
      plots[candidate_pair_id] = plot;
      plot_ids[plot].emplace(candidate_pair_id);
    }

    for (const auto& point : time_series.points) {
      plot_x_limits[plot].Update(point.x);
    }
    plot->AppendTimeSeries(std::move(time_series));
  }
  for (const auto& p : plot_x_limits) {
    auto* plot = p.first;
    const PlotLimits& limits = p.second;
    SetSuggestedXAxis(plot, limits, "Unnormalized Time (s)");
  }

  for (const auto& p : plot_ids) {
    auto candidate_pair_ids_str = JoinCandidatePairIds(p.second);
    p.first->SetTitle("IceSequenceDiagram for candidate_pair_ids " +
                      candidate_pair_ids_str);
  }
}

void IceTransactionAnalyzer::BuildIceTransactions(
    const std::vector<LoggedIceCandidatePairEvent>& log1_events,
    const std::vector<LoggedIceCandidatePairEvent>& log2_events) {
  for (const auto& event : log1_events) {
    IceTransaction& transaction = ice_transactions_[event.transaction_id];
    transaction.log1_candidate_pair_id = event.candidate_pair_id;
    transaction.Update(event, kLogId0);
  }
  for (const auto& event : log2_events) {
    IceTransaction& transaction = ice_transactions_[event.transaction_id];
    transaction.log2_candidate_pair_id = event.candidate_pair_id;
    transaction.Update(event, kLogId1);
  }
}

void IceTransactionAnalyzer::CreateIceTransactionGraphs(
    PlotCollection* plot_collection) {
  using CandidatePairId = uint32_t;
  // CandidatePairIds from different logs should share the same plot if they
  // share a transaction id.
  std::unordered_map<CandidatePairId, Plot*> plots;
  std::unordered_map<Plot*, PlotLimits> plot_x_limits;

  // Used to set plot title. Needs to happen after all transaction ids have
  // been processed since a transaction can be missing one side's candidate pair
  // id.
  std::unordered_map<Plot*, std::unordered_set<CandidatePairId>> plot_ids;

  for (const auto& p : ice_transactions_) {
    TransactionId transaction_id = p.first;
    const IceTransaction& transaction = p.second;
    TimeSeries time_series(std::to_string(transaction_id), LineStyle::kLine,
                           PointStyle::kHighlight);
    // TODO(zstein): Verify this didn't break.
    std::set<CandidatePairId> candidate_pair_ids;
    if (transaction.log1_candidate_pair_id) {
      candidate_pair_ids.insert(*transaction.log1_candidate_pair_id);
    }
    if (transaction.log2_candidate_pair_id) {
      candidate_pair_ids.insert(*transaction.log2_candidate_pair_id);
    }
    bool has_non_check_msg = false;
    auto timestamps = transaction.timestamps();
    for (size_t i = 0; i < timestamps.size(); ++i) {
      if (!timestamps[i]) {
        continue;
      }
      auto ice_timestamp = *timestamps[i];
      float x = ToCallTimeSec(ice_timestamp);
      float y = static_cast<float>(i);
      has_non_check_msg |=
          i != static_cast<size_t>(IceCandidatePairEventType::kCheckSent);
      time_series.points.emplace_back(x, y);
    }

    // Don't plot if we don't have at least one non-check message.
    if (!has_non_check_msg) {
      continue;
    }

    Plot* plot;
    for (auto candidate_pair_id : candidate_pair_ids) {
      plot = plots[candidate_pair_id];
      if (plot) {
        break;
      }
    }
    if (plot == nullptr) {
      plot = plot_collection->AppendNewPlot();
      plot->SetYAxis(0,
                     static_cast<float>(IceCandidatePairEventType::kNumValues),
                     "Numeric IceCandidatePairEvent Type", config_.y_margin,
                     config_.y_margin);
      plot_x_limits[plot];
    }
    for (auto candidate_pair_id : candidate_pair_ids) {
      plots[candidate_pair_id] = plot;
      plot_ids[plot].emplace(candidate_pair_id);
    }

    for (const auto& point : time_series.points) {
      plot_x_limits[plot].Update(point.x);
    }
    plot->AppendTimeSeries(std::move(time_series));
  }
  for (const auto& p : plot_x_limits) {
    auto* plot = p.first;
    const PlotLimits& limits = p.second;
    SetSuggestedXAxis(plot, limits, "Unnormalized Time (s)");
  }
  for (const auto& p : plot_ids) {
    auto candidate_pair_ids_str = JoinCandidatePairIds(p.second);
    p.first->SetTitle("IceTransactions for candidate_pair_ids " +
                      candidate_pair_ids_str);
  }
}

void IceTransactionAnalyzer::CreateIceTransactionStateGraph(
    PlotCollection* plot_collection) {
  using IceTransactionVec = std::vector<IceTransaction>;
  std::map<IceTransaction::ConnectionId, IceTransactionVec> connections;
  for (const auto& transaction : ice_transactions_) {
    IceTransactionVec& transactions =
        connections[transaction.second.connection_id()];
    transactions.push_back(transaction.second);
  }

  Plot* plot = plot_collection->AppendNewPlot();
  plot->SetTitle("IceTransactionStateReached");
  plot->SetYAxis(0, 4, "Stage Reached", config_.y_margin, config_.y_margin);
  PlotLimits x_limits;

  for (const auto& connection : connections) {
    TimeSeries series(ConnectionIdString(connection.first), LineStyle::kNone,
                      PointStyle::kHighlight);
    for (const auto& transaction : connection.second) {
      auto start_time = transaction.start_time();
      if (!start_time) {
        continue;
      }
      // TODO(zstein): Reconsider where on the x axis to draw this.
      float x = ToCallTimeSec(*start_time);
      x_limits.Update(x);
      float y = transaction.stage_reached();
      series.points.emplace_back(x, y);
    }

    std::sort(series.points.begin(), series.points.end(),
              CompareTimeSeriesPoint);

    plot->AppendTimeSeries(std::move(series));
  }
  SetSuggestedXAxis(plot, x_limits, "Unnormalized Time (s)");
}

void IceTransactionAnalyzer::CreateIceTransactionRttGraphs(
    PlotCollection* plot_collection) {
  using IceTransactionVec = std::vector<IceTransaction>;
  std::map<IceTransaction::ConnectionId, IceTransactionVec> connections;
  for (const auto& transaction : ice_transactions_) {
    IceTransactionVec& transactions =
        connections[transaction.second.connection_id()];
    transactions.push_back(transaction.second);
  }

  for (const auto& connection : connections) {
    TimeSeries series("", LineStyle::kNone, PointStyle::kHighlight);
    PlotLimits x_limits, y_limits;
    for (const auto& transaction : connection.second) {
      auto start_time = transaction.start_time();
      if (!start_time) {
        continue;
      }
      auto end_time = transaction.end_time();
      float x = ToCallTimeSec(*start_time);
      x_limits.Update(x);
      float y = end_time.has_value()
                    ? end_time->log_time_ms - start_time->log_time_ms
                    : 0;
      y_limits.Update(y);
      series.points.emplace_back(x, y);
    }

    // Only plot if some transaction completed.
    if (y_limits.max() != 0 &&
        y_limits.max() != -std::numeric_limits<float>::max()) {
      Plot* plot = plot_collection->AppendNewPlot();
      plot->SetTitle("IceTransaction RTT for candidate_pair_ids " +
                     ConnectionIdString(connection.first));
      SetSuggestedXAxis(plot, x_limits, "Unnormalized Time (s)");
      plot->SetSuggestedYAxis(0, y_limits.max(), "RTT (ms)", config_.y_margin,
                              config_.y_margin);
      plot->AppendTimeSeries(std::move(series));
    }
  }
}

float IceTransactionAnalyzer::ToCallTimeSec(IceTimestamp timestamp) {
  // Offset moves timestamps from log2 in to log1 time, so always subtract log1
  // offset here.
  int64_t log_time_ms = timestamp.log_time_ms - config_.log1_first_timestamp_ms;
  if (timestamp.log_id == kLogId1) {
    log_time_ms -= config_.clock_offset_ms;
  }
  return static_cast<float>(log_time_ms) / kNumMillisecsPerSec;
}

void IceTransactionAnalyzer::SetSuggestedXAxis(Plot* plot,
                                               const PlotLimits& limits,
                                               std::string label) {
  auto x_min_s = config_.use_same_x_axis ? config_.x_min_s : limits.min();
  auto x_max_s = config_.use_same_x_axis ? config_.x_max_s : limits.max();
  plot->SetSuggestedXAxis(x_min_s, x_max_s, label, config_.y_margin,
                          config_.x_margin);
}

}  // namespace webrtc
