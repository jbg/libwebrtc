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
#include <utility>
#include <vector>

#include "rtc_base/logging.h"
#include "rtc_tools/event_log_visualizer/clock_offset_calculator.h"

namespace webrtc {

namespace {

const int kNumMillisecsPerSec = 1000;

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

IceTransactionAnalyzer::IceTransactionAnalyzer(
    IceTransactionAnalyzerConfig config,
    const IceTransactions& ice_transactions)
    : config_(config), ice_transactions_(ice_transactions) {}

// TODO(zstein): De-duplicate some of the code shared with
// CreateIceTransactionGraphs. The only difference is the y value per point and
// y axis.
void IceTransactionAnalyzer::CreateIceSequenceDiagrams(
    PlotCollection* plot_collection) {
  using CandidatePairId = uint32_t;
  // CandidatePairIds from different logs should share the same plot if they
  // share a transaction id.
  std::unordered_map<CandidatePairId, Plot*> plots;

  // Used to set plot title. Needs to happen after all transaction ids have
  // been processed since a transaction can be missing one side's candidate pair
  // id.
  std::unordered_map<Plot*, std::unordered_set<CandidatePairId>> plot_ids;

  for (const auto& p : ice_transactions_.ice_transactions) {
    auto transaction_id = p.first;
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
    }
    for (auto candidate_pair_id : candidate_pair_ids) {
      plots[candidate_pair_id] = plot;
      plot_ids[plot].emplace(candidate_pair_id);
    }

    plot->AppendTimeSeries(std::move(time_series));
  }

  for (const auto& p : plot_ids) {
    SetSuggestedXAxis(p.first, "Unnormalized Time (s)");
    auto candidate_pair_ids_str = JoinCandidatePairIds(p.second);
    p.first->SetTitle("IceSequenceDiagram for candidate_pair_ids " +
                      candidate_pair_ids_str);
  }
}

void IceTransactionAnalyzer::CreateIceTransactionGraphs(
    PlotCollection* plot_collection) {
  using CandidatePairId = uint32_t;
  // CandidatePairIds from different logs should share the same plot if they
  // share a transaction id.
  std::unordered_map<CandidatePairId, Plot*> plots;

  // Used to set plot title. Needs to happen after all transaction ids have
  // been processed since a transaction can be missing one side's candidate pair
  // id.
  std::unordered_map<Plot*, std::unordered_set<CandidatePairId>> plot_ids;

  for (const auto& p : ice_transactions_.ice_transactions) {
    auto transaction_id = p.first;
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
    }
    for (auto candidate_pair_id : candidate_pair_ids) {
      plots[candidate_pair_id] = plot;
      plot_ids[plot].emplace(candidate_pair_id);
    }

    plot->AppendTimeSeries(std::move(time_series));
  }
  for (const auto& p : plot_ids) {
    SetSuggestedXAxis(p.first, "Unnormalized Time (s)");
    auto candidate_pair_ids_str = JoinCandidatePairIds(p.second);
    p.first->SetTitle("IceTransactions for candidate_pair_ids " +
                      candidate_pair_ids_str);
  }
}

void IceTransactionAnalyzer::CreateIceTransactionStateGraph(
    PlotCollection* plot_collection) {
  using IceTransactionVec = std::vector<IceTransaction>;
  std::map<IceTransaction::ConnectionId, IceTransactionVec> connections;
  for (const auto& transaction : ice_transactions_.ice_transactions) {
    IceTransactionVec& transactions =
        connections[transaction.second.connection_id()];
    transactions.push_back(transaction.second);
  }

  Plot* plot = plot_collection->AppendNewPlot();
  plot->SetTitle("IceTransactionStateReached");
  plot->SetYAxis(0, 4, "Stage Reached", config_.y_margin, config_.y_margin);

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
      float y = transaction.stage_reached();
      series.points.emplace_back(x, y);
    }

    std::sort(series.points.begin(), series.points.end(),
              CompareTimeSeriesPoint);

    plot->AppendTimeSeries(std::move(series));
  }
  SetSuggestedXAxis(plot, "Unnormalized Time (s)");
}

void IceTransactionAnalyzer::CreateIceTransactionRttGraphs(
    PlotCollection* plot_collection) {
  using IceTransactionVec = std::vector<IceTransaction>;
  std::map<IceTransaction::ConnectionId, IceTransactionVec> connections;
  for (const auto& transaction : ice_transactions_.ice_transactions) {
    IceTransactionVec& transactions =
        connections[transaction.second.connection_id()];
    transactions.push_back(transaction.second);
  }

  for (const auto& connection : connections) {
    TimeSeries series("", LineStyle::kNone, PointStyle::kHighlight);
    bool has_complete_transaction = false;
    for (const auto& transaction : connection.second) {
      auto start_time = transaction.start_time();
      if (!start_time) {
        continue;
      }
      auto end_time = transaction.end_time();
      float x = ToCallTimeSec(*start_time);
      float y = end_time.has_value()
                    ? end_time->log_time_ms - start_time->log_time_ms
                    : 0;
      series.points.emplace_back(x, y);
      has_complete_transaction |= end_time.has_value();
    }

    // Only plot if some transaction completed.
    if (has_complete_transaction) {
      Plot* plot = plot_collection->AppendNewPlot();
      plot->SetTitle("IceTransaction RTT for candidate_pair_ids " +
                     ConnectionIdString(connection.first));
      plot->AppendTimeSeries(std::move(series));
      SetSuggestedXAxis(plot, "Unnormalized Time (s)");
      plot->SetSuggestedYAxis(0, 0, "RTT (ms)", config_.y_margin,
                              config_.y_margin);
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

void IceTransactionAnalyzer::SetSuggestedXAxis(Plot* plot, std::string label) {
  auto x_min_s = config_.use_same_x_axis ? config_.x_min_s
                                         : std::numeric_limits<float>::max();
  auto x_max_s = config_.use_same_x_axis ? config_.x_max_s
                                         : -std::numeric_limits<float>::max();
  plot->SetSuggestedXAxis(x_min_s, x_max_s, label, config_.x_margin,
                          config_.x_margin);
}

}  // namespace webrtc
